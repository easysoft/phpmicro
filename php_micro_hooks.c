/*
micro SAPI for PHP - php_micro_hooks.c
micro hooks for multi kinds of hooking

Copyright 2020 Longyan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "php.h"

#include "php_micro.h"
#include "php_micro_fileinfo.h"

/* ======== things for ini set ======== */

// original zend_post_startup_cb holder
static int (* micro_zend_post_startup_cb_orig)(void) = NULL;
/*
*	micro_post_mstartup - post mstartup callback called as zend_post_startup_cb
*	used to add ini_entries without additional_modules
*/
int micro_post_mstartup(void){
	dbgprintf("start reg inientries\n");
	const zend_ini_entry_def micro_ini_entries[] = {
		ZEND_INI_ENTRY(PHP_MICRO_INIENTRY(php_binary), "", ZEND_INI_PERDIR|ZEND_INI_SYSTEM, NULL)
	{0}};
	int ret = zend_register_ini_entries(micro_ini_entries, 0);
	if(SUCCESS != ret){
		return ret;
	}
	if(NULL != micro_zend_post_startup_cb_orig){
		return micro_zend_post_startup_cb_orig();
	}
	return ret;
}
/*
*	micro_register_post_startup_cb - register post mstartup callback
*/
int micro_register_post_startup_cb(void){
	if(NULL != zend_post_startup_cb){
		micro_zend_post_startup_cb_orig = (void*)zend_post_startup_cb;
	}
	zend_post_startup_cb = (void*) micro_post_mstartup;
	return SUCCESS;
}

/* ======== things for hooking php_stream ======== */
// my own ops struct
typedef struct _micro_php_stream_ops {
    php_stream_ops ops;
    const php_stream_ops * ops_orig;
} micro_php_stream_ops;

// use original ops as ps->ops
#define orig_ops(myops, ps) \
    const php_stream_ops * myops = ps->ops; \
    ps->ops = ((const micro_php_stream_ops*)(ps->ops))->ops_orig;
// use with-offset ops as ps->ops
#define ours_ops(ps) \
    ps->ops = myops;
#define ret_orig(rtyp, name, stream, args) do{\
    orig_ops(myops, stream);\
    rtyp ret = stream->ops->name(stream args);\
    ours_ops(stream);\
    return ret;\
} while (0)
#define with_args(...) , __VA_ARGS__
#define nope

/* ops proxies here
*   why there're many proxies:
*   php DO NOT have a explcit standard applied which
*       limits one php_stream op function (like ops->stat) MUST NOT call another one,
*       or limits one php_stream op function MUST NOT use its own modified php_stream_obs (like what i do).
*   so we use these proxies to call original operation functions
*/
/* stdio like functions - these are mandatory! */
static ssize_t micro_plain_files_write(php_stream *stream, const char *buf, size_t count){
    ret_orig(ssize_t, write, stream, with_args(buf, count));
}
static ssize_t micro_plain_files_read(php_stream *stream, char *buf, size_t count){
    ret_orig(ssize_t, read, stream, with_args(buf, count));
}
static int micro_plain_files_flush(php_stream *stream){
    ret_orig(int, flush, stream, nope);
}
/* these are optional */
static int micro_plain_files_cast(php_stream *stream, int castas, void **_ret){
    ret_orig(int, cast, stream, with_args(castas, _ret));
}
static int micro_plain_files_stat(php_stream *stream, php_stream_statbuf *ssb){
    ret_orig(int, stat, stream, with_args(ssb));
}
#undef ret_orig
#undef with_args
#undef nope
/* end of ops proxies */

/* stream ops hookers */
/*
*	micro_plain_files_set_option - php_stream sef_option op with offset
*	 to fix mmap-like behaiver
*/
static int micro_plain_files_set_option(php_stream *stream, int option, int value, void *ptrparam){
    void* myptrparam = ptrparam;

    if(option == PHP_STREAM_OPTION_MMAP_API && value == PHP_STREAM_MMAP_MAP_RANGE){
        dbgprintf("trying mmap, let us mask it!\n");
        php_stream_mmap_range * range = myptrparam;
        if(
            PHP_STREAM_MAP_MODE_READWRITE == range->mode ||
            PHP_STREAM_MAP_MODE_SHARED_READWRITE == range->mode
        ){
            // self should not be writeable
            return PHP_STREAM_OPTION_RETURN_ERR;
        }
        range->offset = range->offset + micro_get_sfx_filesize();
    }
    orig_ops(myops, stream);
    int ret = stream->ops->set_option(stream, option, value, myptrparam);
    ours_ops(stream);
    return ret;
}
/*
*	micro_plain_files_seek_with_offset - php_stream seek op with offset
*	 return -1 for failed or 0 for success (behaives like fseek)
*/
static int micro_plain_files_seek_with_offset(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset){
    dbgprintf("seeking %zd with offset %d whence %d\n", offset, micro_get_sfx_filesize(), whence);
    int ret = -1;
    zend_off_t realoffset;

    orig_ops(myops, stream);
    if(SEEK_SET == whence){
        ret = stream->ops->seek(stream, offset + micro_get_sfx_filesize(), whence, &realoffset);
    }else{
        ret = stream->ops->seek(stream, offset, whence, &realoffset);
    }
    ours_ops(stream);
    if(-1 == ret){
        return -1;
    }
    if(realoffset < micro_get_sfx_filesize()){
        php_error_docref(NULL, E_WARNING, "Seek on self stream failed");
		return -1;
    }
    *newoffset = realoffset - micro_get_sfx_filesize();
    return ret;
}

/*
*	micro_plain_files_stat_with_offset - php_stream stat op with offset
*/
static int micro_plain_files_stat_with_offset(php_stream *stream, php_stream_statbuf *ssb){
    int ret = -1;

    orig_ops(myops, stream);
    ret = stream->ops->stat(stream, ssb);
    ours_ops(stream);
    if(-1 == ret){
        return -1;
    }
    dbgprintf("stating withoffset %zd -> %zd\n", ssb->sb.st_size, ssb->sb.st_size-micro_get_sfx_filesize());
    ssb->sb.st_size -= micro_get_sfx_filesize();
    return ret;
}

/*
*	micro_plain_files_close_with_offset - php_stream close destroyer
*/
static int micro_plain_files_close_with_offset(php_stream *stream, int close_handle){
    dbgprintf("closing with-offset file %p\n", stream);

    orig_ops(myops, stream);
    pefree((void*)myops, stream->is_persistent);
    //free(myops);
    int ret = stream->ops->close(stream, close_handle);
    return ret;
}

#undef orig_ops
#undef ours_ops
/* end of stream ops hookers */

/*
*   micro_modify_ops_with_offset - modify a with-offset ops struct, the argument ps must be created
*/
static inline int micro_modify_ops_with_offset(php_stream * ps, int mod_stat){
    dbgprintf("compare %p, %p\n", ps->ops->close, micro_plain_files_close_with_offset);
    if (
        ps->ops->close == micro_plain_files_close_with_offset
    ){
        dbgprintf("offset alread set, skip it\n");
        return FAILURE;
    }
    micro_php_stream_ops *ret = pemalloc(sizeof(*ret), ps->is_persistent);
    //micro_php_stream_ops *ret = malloc(sizeof(*ret));
    (*ret).ops = (php_stream_ops){
        // set with-offset op handlers
        .close = micro_plain_files_close_with_offset,
        .seek = NULL == ps->ops->seek ? NULL : micro_plain_files_seek_with_offset,
        .stat = NULL == ps->ops->stat ? NULL : 
            (0 == mod_stat ? micro_plain_files_stat : micro_plain_files_stat_with_offset),
        // set proxies
        .write = micro_plain_files_write,
        .read = micro_plain_files_read,
        .flush = micro_plain_files_flush,
        .cast = NULL == ps->ops->cast ? NULL : micro_plain_files_cast,
        .set_option = NULL == ps->ops->set_option ? NULL : micro_plain_files_set_option,
        // set original label
        .label = ps->ops->label,
    };
    ret->ops_orig = ps->ops;
    dbgprintf("assigning psop %p\n", ret);
    ps->ops = (const php_stream_ops*)ret;
    return SUCCESS;
}

// holder for original php_plain_files_wrapper_ops
const php_stream_wrapper_ops* micro_plain_files_wops_orig = NULL;
static inline int initial_seek(php_stream * ps);
/*
*   micro_plain_files_opener - stream_opener that modify ops according to filename
*   replaces php_plain_files_stream_opener
*   should be called after micro_fileinfo_init
*/
static php_stream *micro_plain_files_opener(php_stream_wrapper *wrapper, const char *filename, const char *mode,
			int options, zend_string **opened_path, php_stream_context *context STREAMS_DC){
    dbgprintf("opening file %s like plain file\n", filename);
    if(NULL == micro_plain_files_wops_orig){
        // this should never happen
        return NULL;
    }
    static const char * self_filename_slashed = NULL;
    size_t self_filename_slashed_len = 0;
    if(NULL == self_filename_slashed){
        self_filename_slashed = micro_slashize(micro_get_filename());
        self_filename_slashed_len = strlen(self_filename_slashed);
    }
    php_stream * ps = micro_plain_files_wops_orig->stream_opener(wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    if(NULL == ps){
        return ps;
    }
    const char* filename_slashed = micro_slashize(filename);
    if(0 == strcmp(filename_slashed, self_filename_slashed)){  
        dbgprintf("opening self via php_stream, hook it\n");
        if(SUCCESS == micro_modify_ops_with_offset(ps, 1)){
            initial_seek(ps);
        }
    }
    free((void*)filename_slashed);
    dbgprintf("done opening plain file %p\n", ps);
    return ps;
}

/*
*   micro_plain_files_url_stater - url_stater that modify stat according to filename
*   replaces php_plain_files_url_stater
*   should be called after micro_fileinfo_init
*/
static int micro_plain_files_url_stater(php_stream_wrapper *wrapper, const char *url, int flags, php_stream_statbuf *ssb, php_stream_context *context){
    dbgprintf("stating file %s like plain file\n", url);
    if(NULL == micro_plain_files_wops_orig){
        // this should never happen
        return FAILURE;
    }
    static const char * self_filename_slashed = NULL;
    size_t self_filename_slashed_len = 0;
    if(NULL == self_filename_slashed){
        self_filename_slashed = micro_slashize(micro_get_filename());
        self_filename_slashed_len = strlen(self_filename_slashed);
    }
    int ret = micro_plain_files_wops_orig->url_stat(wrapper, url, flags, ssb, context);
    if(SUCCESS != ret){
        return ret;
    }
    const char* filename_slashed = micro_slashize(url);
    if(0 == strcmp(filename_slashed, self_filename_slashed)){  
        dbgprintf("stating self via plain file wops, hook it\n");
        ssb->sb.st_size -= micro_get_sfx_filesize();
    }
    free((void*)filename_slashed);
    return ret;
}

// with-offset wrapper ops to replace php_plain_files_wrapper.wops
static php_stream_wrapper_ops micro_plain_files_wops_with_offset ;

/*
*	micro_hook_plain_files_wops - hook plain file wrapper php_plain_files_wrapper
*/
int micro_hook_plain_files_wops(void){
    micro_plain_files_wops_orig = php_plain_files_wrapper.wops;
    memcpy(&micro_plain_files_wops_with_offset, micro_plain_files_wops_orig, sizeof(*micro_plain_files_wops_orig));
    micro_plain_files_wops_with_offset.stream_opener = micro_plain_files_opener;
    micro_plain_files_wops_with_offset.url_stat = micro_plain_files_url_stater;
    //micro_plain_files_wops_with_offset.stream_opener = micro_php_stream_closer;
    php_plain_files_wrapper.wops = &micro_plain_files_wops_with_offset;
    return SUCCESS;
}

/* ======== things for hooking url_stream ======== */

HashTable reregistered_protos = {0};
int reregistered_protos_inited = 0;

typedef struct _micro_reregistered_proto {
    php_stream_wrapper * mwrapper;
    php_stream_wrapper_ops * mwops;
    php_stream_wrapper * wrapper_orig;
    char proto[1];
} micro_reregistered_proto;
// no wrapper ops proxies here

static inline int initial_seek(php_stream * ps);
/*
*   micro_wrapper_stream_opener - modify ops according to filename
*   replaces someproto_wrapper_open_url
*   should be called after micro_fileinfo_init
*/
static php_stream *micro_wrapper_stream_opener(php_stream_wrapper *wrapper, const char *filename, const char *mode,
			int options, zend_string **opened_path, php_stream_context *context STREAMS_DC){
    dbgprintf("opening file %s like url\n", filename);

    micro_reregistered_proto* mdata = zend_hash_str_find_ptr(&reregistered_protos, (void*)wrapper, sizeof(*wrapper));
    if(NULL == mdata){
        // this should never happen
        return NULL;
    }
    static const char * self_filename_slashed = NULL;
    static size_t self_filename_slashed_len = 0;
    if(NULL == self_filename_slashed){
        self_filename_slashed = micro_slashize(micro_get_filename());
        self_filename_slashed_len = strlen(self_filename_slashed);
    }
    php_stream * ps = mdata->wrapper_orig->wops->stream_opener(wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    if(NULL == ps){
        return ps;
    }
    const char* filename_slashed = micro_slashize(filename);
    if(
        strstr(filename, "://") &&
        0 == strncmp(strstr(filename_slashed, "://") + 3, self_filename_slashed, self_filename_slashed_len)
    ){
        dbgprintf("stream %s is in self\n", filename);
        if(SUCCESS == micro_modify_ops_with_offset(ps, 0)){
            initial_seek(ps);
        }
    }
    free((void*)filename_slashed);
    dbgprintf("done opening scheme file %p\n", ps);
    return ps;
}

/*
*	micro_reregister_proto - hook some:// protocol
*   should be called after mstartup, before start execution
*/
int micro_reregister_proto(const char* proto){
    if(0 == reregistered_protos_inited){
        zend_hash_init(&reregistered_protos, 0, NULL, ZVAL_PTR_DTOR, 1);
        reregistered_protos_inited = 1;
    }
    int ret = SUCCESS;
    HashTable * ht = php_stream_get_url_stream_wrappers_hash_global();
    php_stream_wrapper * wrapper = zend_hash_find_ptr(ht, zend_string_init_interned(proto, strlen(proto), 1));
    if(NULL == wrapper){
        // no some:// found
        return SUCCESS;
    }
    if(SUCCESS != (ret = php_unregister_url_stream_wrapper(proto))){
        dbgprintf("failed unregister proto %s\n", proto);
        return ret;
    }
    
    if(NULL == wrapper->wops->stream_opener){
        // no stream_opener, just say success
        return ret;
    }
    php_stream_wrapper_ops *mwops = malloc(sizeof(*mwops));
    php_stream_wrapper *mwrapper = malloc(sizeof(*mwrapper));
    mwrapper->wops = mwops;
    mwrapper->abstract = wrapper->abstract;
    mwrapper->is_url = wrapper->is_url;

    memcpy(mwops, wrapper->wops, sizeof(*mwops));
    mwops->stream_opener = micro_wrapper_stream_opener;

    // re-register it
    if(SUCCESS != (ret = php_register_url_stream_wrapper(proto, mwrapper))){
        dbgprintf("failed reregister proto %s\n", proto);
        free(mwops);
        free(mwrapper);
        return ret;
    };
    micro_reregistered_proto* pproto = malloc(sizeof(*pproto)+strlen(proto));
    pproto->mwops = mwops;
    pproto->mwrapper = mwrapper;
    pproto->wrapper_orig = wrapper;
    memcpy(pproto->proto, proto, strlen(proto)+1);
    zend_hash_str_add_ptr(&reregistered_protos, (void*)mwrapper, sizeof(*mwrapper), pproto);
    return SUCCESS;
}

/*
*	micro_free_reregistered_protos - remove hook of protocol schemes
*   should be called before mshutdown, after rshutdown
*/
int micro_free_reregistered_protos(void){
    int finalret = SUCCESS;
    ZEND_HASH_REVERSE_FOREACH_PTR(&reregistered_protos, micro_reregistered_proto* mdata)
        int ret = SUCCESS;
        const char * proto = mdata->proto;
        dbgprintf("free reregistered proto %s\n", proto);
        if(SUCCESS != (ret = php_unregister_url_stream_wrapper(proto))){
            dbgprintf("failed unregister reregistered proto %s\n", proto);
            finalret = ret;
            continue;
        }
        if(SUCCESS != (ret = php_register_url_stream_wrapper(proto, mdata->wrapper_orig))){
            dbgprintf("failed restore reregistered proto %s\n", proto);
            finalret = ret;
            continue;
        }
        free(mdata->mwops);
        free(mdata->mwrapper);
        free(mdata);
    ZEND_HASH_FOREACH_END_DEL();
    return finalret;
}

static inline int initial_seek(php_stream * ps){
    dbgprintf("initial seeking\n");
    zend_off_t dummy;
    if(0 == ps->position){
        // not appending mode
        ps->ops->seek(ps, 0, SEEK_SET, &dummy);
    }else if(0 < ps->position){
        // appending mode
        // this will only called after micro_fileinfo_init,
        //  so it's sure thatself size wont be smaller then sfx size.
        ps->position -= micro_get_sfx_filesize();
        ps->ops->seek(ps, ps->position, SEEK_SET, &dummy);
    }else{
        // self should be seekable, if not, why?
        abort();
    }
    return SUCCESS;
}
