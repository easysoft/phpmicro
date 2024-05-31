/*
micro SAPI for PHP - php_micro_hooks.c
micro hooks for multi kinds of hooking

Copyright 2020 Longyan
Copyright 2022 Yun Dou <dixyes@gmail.com>

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
static int (*micro_zend_post_startup_cb_orig)(void) = NULL;
/*
 *	micro_post_mstartup - post mstartup callback called as zend_post_startup_cb
 *	used to add ini_entries without additional_modules
 */
int micro_post_mstartup(void) {
    dbgprintf("start reg inientries\n");
    const zend_ini_entry_def micro_ini_entries[] = {
        ZEND_INI_ENTRY(PHP_MICRO_INIENTRY(php_binary), "", ZEND_INI_PERDIR | ZEND_INI_SYSTEM, NULL){0},
    };
    int ret = zend_register_ini_entries(micro_ini_entries, 0);
    if (SUCCESS != ret) {
        return ret;
    }
    if (NULL != micro_zend_post_startup_cb_orig) {
        return micro_zend_post_startup_cb_orig();
    }
    return ret;
}
/*
 *	micro_register_post_startup_cb - register post mstartup callback
 */
int micro_register_post_startup_cb(void) {
    if (NULL != zend_post_startup_cb) {
        micro_zend_post_startup_cb_orig = (void *)zend_post_startup_cb;
    }
    zend_post_startup_cb = (void *)micro_post_mstartup;
    return SUCCESS;
}

/* ======== things for hooking php_stream ======== */
// my own ops struct
typedef struct _micro_php_stream_ops {
    php_stream_ops ops;
    const php_stream_ops *ops_orig;
} micro_php_stream_ops;

// use original ops as ps->ops
#define orig_ops(myops, ps) \
    const php_stream_ops *myops = ps->ops; \
    ps->ops = ((const micro_php_stream_ops *)(ps->ops))->ops_orig;
// use with-offset ops as ps->ops
#define ours_ops(ps) ps->ops = myops;
#define ret_orig(rtyp, name, stream, args) \
    do { \
        orig_ops(myops, stream); \
        rtyp ret = stream->ops->name(stream args); \
        ours_ops(stream); \
        return ret; \
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
static ssize_t micro_plain_files_write(php_stream *stream, const char *buf, size_t count) {
    ret_orig(ssize_t, write, stream, with_args(buf, count));
}
static int micro_plain_files_flush(php_stream *stream) {
    ret_orig(int, flush, stream, nope);
}
/* these are optional */
static int micro_plain_files_cast(php_stream *stream, int castas, void **_ret) {
    ret_orig(int, cast, stream, with_args(castas, _ret));
}
static int micro_plain_files_stat(php_stream *stream, php_stream_statbuf *ssb) {
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
static int micro_plain_files_set_option(php_stream *stream, int option, int value, void *ptrparam) {
    void *myptrparam = ptrparam;
    size_t limit;

    if (option == PHP_STREAM_OPTION_MMAP_API && value == PHP_STREAM_MMAP_MAP_RANGE) {
        dbgprintf("trying mmap self, let us mask it!\n");
        php_stream_mmap_range *range = myptrparam;
        if (PHP_STREAM_MAP_MODE_READWRITE == range->mode || PHP_STREAM_MAP_MODE_SHARED_READWRITE == range->mode) {
            // self should not be writeable
            return PHP_STREAM_OPTION_RETURN_ERR;
        }
        range->offset = range->offset + micro_get_sfxsize();
        /**
         * linux mmap(2) manpage said:
         * EOVERFLOW
         *     The file is a regular file and 
         *     the value of off plus len exceeds the offset maximum established in the open file description
         *     associated with fildes.
         * 
         */
        if ((limit = micro_get_sfxsize_limit()) != 0) {
            if (range->offset + range->length > limit) {
#ifdef EOVERFLOW // is this necessary?
                errno = EOVERFLOW;
#else
                errno = EINVAL;
#endif // EOVERFLOW
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
        }
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
static int micro_plain_files_seek_with_offset(
    php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset) {
    dbgprintf("seeking %zd with sfxsize %d limit %d whence %d\n",
        offset, micro_get_sfxsize(), micro_get_sfxsize_limit(), whence);
    int ret = -1;
    zend_off_t want_offset, real_offset;
    zend_off_t sfxsize, limit;

    sfxsize = micro_get_sfxsize();
    limit = micro_get_sfxsize_limit();

    orig_ops(myops, stream);
    switch(whence) {
        case SEEK_SET:
            want_offset = offset + sfxsize;
            break;
        case SEEK_CUR:
            ret = stream->ops->seek(stream, 0, SEEK_CUR, &want_offset);
            if (-1 == ret) {
                goto error;
            }
            want_offset += offset;
            break;
        case SEEK_END:
            if (0 == limit) {
                ret = stream->ops->seek(stream, 0, SEEK_END, &limit);
                if (-1 == ret) {
                    goto error;
                }
            }
            want_offset = limit + offset;
            break;
    }
    dbgprintf("want offset: %zd\n", want_offset);

    if (want_offset < sfxsize) {
        // seek before start
        goto error;
    }

    ret = stream->ops->seek(stream, want_offset, SEEK_SET, &real_offset);
    if (-1 == ret) {
        php_error_docref(NULL, E_WARNING, "Seek on self stream failed");
        goto error;
    }

    ours_ops(stream);

    *newoffset = real_offset - sfxsize;

    dbgprintf("new offset: %zd\n", *newoffset);
    return 0;
    error:
    ours_ops(stream);
    // php_error_docref(NULL, E_WARNING, "Seek on self stream failed");
    return -1;
}

/*
 *	micro_plain_files_stat_with_offset - php_stream stat op with offset
 */
static int micro_plain_files_stat_with_offset(php_stream *stream, php_stream_statbuf *ssb) {
    int ret = -1;
    uint32_t limit = 0;

    orig_ops(myops, stream);
    ret = stream->ops->stat(stream, ssb);
    ours_ops(stream);
    if (-1 == ret) {
        return -1;
    }

    dbgprintf("stating real size %zd\n", ssb->sb.st_size);
    if ((limit = micro_get_sfxsize_limit()) > 0) {
        ssb->sb.st_size = limit - micro_get_sfxsize();
    } else {
        ssb->sb.st_size -= micro_get_sfxsize();
    }
    dbgprintf("stating with offset %zd\n", ssb->sb.st_size);
    return ret;
}

/*
 * micro_plain_files_read_with_offset - php_stream read op with offset
 */
static ssize_t micro_plain_files_read_with_offset(php_stream *stream, char *buf, size_t count) {
    ssize_t ret = -1;
    zend_off_t current;
    size_t limit;

    orig_ops(myops, stream);
    ret = stream->ops->seek(stream, 0, SEEK_CUR, &current);
    if (-1 == ret || current < 0) {
        ret = -1;
        goto error;
    }

    if ((limit = micro_get_sfxsize_limit()) > 0) {
        if (((size_t)current) + count > limit) {
            count = limit - current;
        }
    }
    ret = stream->ops->read(stream, buf, count);
    error:
    ours_ops(stream);
    return ret;
}

/*
 *	micro_plain_files_close_with_offset - php_stream close destroyer
 */
static int micro_plain_files_close_with_offset(php_stream *stream, int close_handle) {
    dbgprintf("closing with-offset file %p\n", stream);

    orig_ops(myops, stream);
    pefree((void *)myops, stream->is_persistent);
    // free(myops);
    int ret = stream->ops->close(stream, close_handle);
    return ret;
}

#undef orig_ops
#undef ours_ops
/* end of stream ops hookers */

/*
 *   micro_modify_ops_with_offset - modify a with-offset ops struct, the argument ps must be created
 */
static inline int micro_modify_ops_with_offset(php_stream *ps, int mod_stat) {
    dbgprintf("compare %p, %p\n", ps->ops->close, micro_plain_files_close_with_offset);
    if (ps->ops->close == micro_plain_files_close_with_offset) {
        dbgprintf("offset alread set, skip it\n");
        return FAILURE;
    }
    micro_php_stream_ops *ret = pemalloc(sizeof(*ret), ps->is_persistent);
    // micro_php_stream_ops *ret = malloc(sizeof(*ret));
    (*ret).ops = (php_stream_ops){
        // set with-offset op handlers
        .close = micro_plain_files_close_with_offset,
        .seek = NULL == ps->ops->seek ? NULL : micro_plain_files_seek_with_offset,
        .stat = NULL == ps->ops->stat ? NULL
                                      : (0 == mod_stat ? micro_plain_files_stat : micro_plain_files_stat_with_offset),
        // set proxies
        .write = micro_plain_files_write,
        .read = micro_plain_files_read_with_offset,
        .flush = micro_plain_files_flush,
        .cast = NULL == ps->ops->cast ? NULL : micro_plain_files_cast,
        .set_option = NULL == ps->ops->set_option ? NULL : micro_plain_files_set_option,
        // set original label
        .label = ps->ops->label,
    };
    ret->ops_orig = ps->ops;
    dbgprintf("assigning psop %p\n", ret);
    ps->ops = (const php_stream_ops *)ret;
    return SUCCESS;
}

// holder for original php_plain_files_wrapper_ops
const php_stream_wrapper_ops *micro_plain_files_wops_orig = NULL;
static inline int initial_seek(php_stream *ps);
/*
 *   micro_plain_files_opener - stream_opener that modify ops according to filename
 *   replaces php_plain_files_stream_opener
 *   should be called after micro_fileinfo_init
 */
static php_stream *micro_plain_files_opener(php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC) {
    dbgprintf("opening file %s like plain file\n", filename);
    if (NULL == micro_plain_files_wops_orig) {
        // this should never happen
        return NULL;
    }
    static const char *self_filename_slashed = NULL;
    if (NULL == self_filename_slashed) {
        self_filename_slashed = micro_slashize(micro_get_filename());
    }
    php_stream *ps = micro_plain_files_wops_orig->stream_opener(
        wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    if (NULL == ps) {
        return ps;
    }
    const char *filename_slashed = micro_slashize(filename);
    if (0 == strcmp(filename_slashed, self_filename_slashed)) {
        dbgprintf("opening self via php_stream, hook it\n");
        if (SUCCESS == micro_modify_ops_with_offset(ps, 1)) {
            initial_seek(ps);
        }
    }
    free((void *)filename_slashed);
    dbgprintf("done opening plain file %p\n", ps);
    return ps;
}

/*
 *   micro_plain_files_url_stater - url_stater that modify stat according to filename
 *   replaces php_plain_files_url_stater
 *   should be called after micro_fileinfo_init
 */
static int micro_plain_files_url_stater(
    php_stream_wrapper *wrapper, const char *url, int flags, php_stream_statbuf *ssb, php_stream_context *context) {
    dbgprintf("stating file %s like plain file\n", url);
    if (NULL == micro_plain_files_wops_orig) {
        // this should never happen
        return FAILURE;
    }
    static const char *self_filename_slashed = NULL;
    if (NULL == self_filename_slashed) {
        self_filename_slashed = micro_slashize(micro_get_filename());
    }
    int ret = micro_plain_files_wops_orig->url_stat(wrapper, url, flags, ssb, context);
    if (SUCCESS != ret) {
        return ret;
    }
    const char *filename_slashed = micro_slashize(url);
    if (0 == strcmp(filename_slashed, self_filename_slashed)) {
        dbgprintf("stating self via plain file wops, hook it\n");
        ssb->sb.st_size -= micro_get_sfxsize();
    }
    free((void *)filename_slashed);
    return ret;
}

// with-offset wrapper ops to replace php_plain_files_wrapper.wops
static php_stream_wrapper_ops micro_plain_files_wops_with_offset;

/*
 *	micro_hook_plain_files_wops - hook plain file wrapper php_plain_files_wrapper
 */
int micro_hook_plain_files_wops(void) {
    micro_plain_files_wops_orig = php_plain_files_wrapper.wops;
    memcpy(&micro_plain_files_wops_with_offset, micro_plain_files_wops_orig, sizeof(*micro_plain_files_wops_orig));
    micro_plain_files_wops_with_offset.stream_opener = micro_plain_files_opener;
    micro_plain_files_wops_with_offset.url_stat = micro_plain_files_url_stater;
    // micro_plain_files_wops_with_offset.stream_opener = micro_php_stream_closer;
    php_plain_files_wrapper.wops = &micro_plain_files_wops_with_offset;
    return SUCCESS;
}

/* ======== things for hooking url_stream ======== */

HashTable reregistered_protos = {0};
int reregistered_protos_inited = 0;

typedef struct _micro_reregistered_proto {
    php_stream_wrapper *modified_wrapper;
    php_stream_wrapper_ops *modified_wops;
    php_stream_wrapper *orig_wrapper;
    char proto[1];
} micro_reregistered_proto;
// no wrapper ops proxies here

static inline int initial_seek(php_stream *ps);
/*
 *   micro_wrapper_stream_opener - modify ops according to filename
 *   replaces someproto_wrapper_open_url
 *   should be called after micro_fileinfo_init
 */
static php_stream *micro_wrapper_stream_opener(php_stream_wrapper *wrapper, const char *filename, const char *mode,
    int options, zend_string **opened_path, php_stream_context *context STREAMS_DC) {
    dbgprintf("opening file %s like url\n", filename);

    micro_reregistered_proto *pproto = zend_hash_str_find_ptr(&reregistered_protos, (void *)wrapper, sizeof(*wrapper));
    if (NULL == pproto) {
        // this should never happen
        return NULL;
    }
    static const char *self_filename_slashed = NULL;
    static size_t self_filename_slashed_len = 0;
    if (NULL == self_filename_slashed) {
        self_filename_slashed = micro_slashize(micro_get_filename());
        self_filename_slashed_len = strlen(self_filename_slashed);
    }
    php_stream *ps = pproto->orig_wrapper->wops->stream_opener(
        wrapper, filename, mode, options, opened_path, context STREAMS_REL_CC);
    if (NULL == ps) {
        return ps;
    }
    const char *filename_slashed = micro_slashize(filename);
    if (strstr(filename, "://") &&
        0 == strncmp(strstr(filename_slashed, "://") + 3, self_filename_slashed, self_filename_slashed_len)) {
        dbgprintf("stream %s is in self\n", filename);
        if (SUCCESS == micro_modify_ops_with_offset(ps, 0)) {
            initial_seek(ps);
        }
    }
    free((void *)filename_slashed);
    dbgprintf("done opening file %p like url\n", ps);
    return ps;
}

#if PHP_VERSION_ID < 80100
#    define zend_string_init_existing_interned zend_string_init_interned
#endif

/*
 *	micro_reregister_proto - hook some:// protocol
 *   should be called after mstartup, before start execution
 */
int micro_reregister_proto(const char *proto) {
    dbgprintf("reregister proto %s\n", proto);

    php_stream_wrapper_ops *modified_wops = NULL;
    php_stream_wrapper *modified_wrapper = NULL;
    int ret = SUCCESS;
    HashTable *ht = php_stream_get_url_stream_wrappers_hash_global();
    php_stream_wrapper *orig_wrapper =
        zend_hash_find_ptr(ht, zend_string_init_existing_interned(proto, strlen(proto), 1));
    if (NULL == orig_wrapper) {
        // no wrapper found
        goto end;
    }

    if (SUCCESS != (ret = php_unregister_url_stream_wrapper(proto))) {
        dbgprintf("failed unregister proto %s\n", proto);
        goto end;
    }

    if (NULL == orig_wrapper->wops->stream_opener) {
        // no stream_opener, just say success
        dbgprintf("proto %s have no stream_opener\n", proto);
        goto end;
    }

    if (0 == reregistered_protos_inited) {
        zend_hash_init(&reregistered_protos, 0, NULL, ZVAL_PTR_DTOR, 1);
        reregistered_protos_inited = 1;
    }
    modified_wops = malloc(sizeof(*modified_wops));
    modified_wrapper = malloc(sizeof(*modified_wrapper));

    modified_wrapper->wops = modified_wops;
    modified_wrapper->abstract = orig_wrapper->abstract;
    modified_wrapper->is_url = orig_wrapper->is_url;

    // copy original to modified
    memcpy(modified_wops, orig_wrapper->wops, sizeof(*modified_wops));

    // modify stream opener
    modified_wops->stream_opener = micro_wrapper_stream_opener;

    // re-register it
    if (SUCCESS != (ret = php_register_url_stream_wrapper(proto, modified_wrapper))) {
        dbgprintf("failed reregister proto %s\n", proto);
        goto end;
    };

    // save info for freeing
    micro_reregistered_proto *pproto = malloc(sizeof(*pproto) + strlen(proto));
    pproto->modified_wops = modified_wops;
    pproto->modified_wrapper = modified_wrapper;
    pproto->orig_wrapper = orig_wrapper;
    memcpy(pproto->proto, proto, strlen(proto) + 1);
    zend_hash_str_add_ptr(&reregistered_protos, (void *)modified_wrapper, sizeof(*modified_wrapper), pproto);
    dbgprintf("reregistered proto (label %s) %s %p => %p %p\n",
        orig_wrapper->wops->label,
        proto,
        orig_wrapper,
        modified_wrapper,
        modified_wops);

    return ret;
end:
    if (modified_wops) {
        free(modified_wops);
    }
    if (modified_wrapper) {
        free(modified_wrapper);
    }
    return ret;
}

#ifdef ZEND_HASH_MAP_REVERSE_FOREACH_PTR
#    define HASH_REVERSE_FOREACH_PTR ZEND_HASH_MAP_REVERSE_FOREACH_PTR
#else
#    define HASH_REVERSE_FOREACH_PTR ZEND_HASH_REVERSE_FOREACH_PTR
#endif

/*
 *	micro_free_reregistered_protos - remove hook of protocol schemes
 *   should be called before mshutdown, after rshutdown
 */
int micro_free_reregistered_protos(void) {
    int final_ret = SUCCESS;
    HASH_REVERSE_FOREACH_PTR(&reregistered_protos, micro_reregistered_proto * pproto)
        int ret = SUCCESS;
        const char *proto = pproto->proto;
        dbgprintf("free reregistered proto %s\n", proto);
        if (SUCCESS != (ret = php_unregister_url_stream_wrapper(proto))) {
            dbgprintf("failed unregister reregistered proto %s\n", proto);
            final_ret = ret;
            continue;
        }
        if (SUCCESS != (ret = php_register_url_stream_wrapper(proto, pproto->orig_wrapper))) {
            dbgprintf("failed restore reregistered proto %s\n", proto);
            final_ret = ret;
            continue;
        }
        free(pproto->modified_wrapper);
        free(pproto->modified_wops);
        free(pproto);
    ZEND_HASH_FOREACH_END_DEL();
    return final_ret;
}

static inline int initial_seek(php_stream *ps) {
    dbgprintf("initial seeking\n");
    zend_off_t dummy;
    if (0 == ps->position) {
        // not appending mode
        ps->ops->seek(ps, 0, SEEK_SET, &dummy);
    } else if (0 < ps->position) {
        // appending mode
        // this will only called after micro_fileinfo_init,
        //  so it's sure thatself size wont be smaller then sfx size.
        ps->position -= micro_get_sfxsize();
        ps->ops->seek(ps, ps->position, SEEK_SET, &dummy);
    } else {
        // self should be seekable, if not, why?
        abort();
    }
    return SUCCESS;
}
