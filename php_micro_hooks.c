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

// original zend_stream_open_function holder
#if PHP_MAJOR_VERSION > 7
zend_result (*micro_zend_stream_open_function_orig)(const char *, zend_file_handle *) = NULL;
#else
int (*micro_zend_stream_open_function_orig)(const char *, zend_file_handle *) = NULL;
#endif

typedef struct _micro_stream_with_offset_t {
    php_stream *stream_orig;
    zend_stream_reader_t   reader_orig;
	zend_stream_fsizer_t   fsizer_orig;
	zend_stream_closer_t   closer_orig;
} micro_stream_with_offset_t;

/*
*	micro_zend_stream_reader - zend_stream reader proxy
*	 return size with offset
*/
static ssize_t micro_zend_stream_reader(void* handle, char* buf, size_t len){
    micro_stream_with_offset_t *swo = handle;
    return swo->reader_orig(swo->stream_orig, buf, len);
}
/*
*	micro_zend_stream_fsizer_with_offset - zend_stream fsizer with offset
*	 return size with offset
*/
static size_t micro_zend_stream_fsizer_with_offset(void* handle){
    micro_stream_with_offset_t *swo = handle;
    size_t ret = swo->fsizer_orig(swo->stream_orig) - micro_get_sfx_filesize();
    if (0 > ret){
        // strange size
        return 0;
    }
    return ret;
}
/*
*	micro_zend_stream_closer - zend_stream closer proxy
*	 return size with offset
*/
static void micro_zend_stream_closer(void* handle){
    micro_stream_with_offset_t *swo = handle;
    swo->closer_orig(swo->stream_orig);
}

typedef struct _micro_stream_fp_with_offset_t {
    FILE *fp_orig;
    zend_stream_reader_t   reader_orig;
	zend_stream_fsizer_t   fsizer_orig;
	zend_stream_closer_t   closer_orig;
} micro_stream_fp_with_offset_t;

/*
*	micro_zend_stream_fp_reader - zend_stream fp reader proxy
*	 return size with offset
*/
static SSIZE_T micro_zend_stream_fp_reader(void* handle, char* buf, size_t len){
    micro_stream_fp_with_offset_t *sfwo = handle;
    return sfwo->reader_orig(sfwo->fp_orig, buf, len);
}
/*
*	micro_zend_stream_fsizer_fp_with_offset - zend_stream fsizer with offset for fp type
*	 return size with offset
*/
static size_t micro_zend_stream_fp_fsizer_with_offset(void* handle){
    micro_stream_fp_with_offset_t *sfwo = handle;
    size_t ret = sfwo->fsizer_orig(sfwo->fp_orig) - micro_get_sfx_filesize();
    if (0 > ret){
        // strange size
        return 0;
    }
    return ret;
}
/*
*	micro_zend_stream_fp_closer - zend_stream fp closer proxy
*	 return size with offset
*/
static void micro_zend_stream_fp_closer(void* handle){
    micro_stream_fp_with_offset_t *sfwo = handle;
    sfwo->closer_orig(sfwo->fp_orig);
}

/*
*	micro_php_stream_seek_with_offset - php_stream seek op with offset
*	 return size with offset
*/
int micro_php_stream_seek_with_offset(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset){
    // micro will only be executed as common file (or phar)
    // so ops->seek will be php_stdiop_seek
    dbgprintf("seeking %zd with offset %d\n", offset, micro_get_sfx_filesize());
    int ret = -1;
    zend_off_t dummy;

    if(SEEK_SET == whence){
        ret = php_stream_stdio_ops.seek(stream, offset - micro_get_sfx_filesize(), whence, &dummy);
    }else{
        ret = php_stream_stdio_ops.seek(stream, offset, whence, &dummy);
    }
    if(dummy < micro_get_sfx_filesize()){
        php_error_docref(NULL, E_WARNING, "Seek on self stream failed");
		return -1;
    }
    if(0 == ret || -1 == ret){
        return ret;
    }
    *newoffset = dummy - micro_get_sfx_filesize();
    return ret - micro_get_sfx_filesize();
}

static php_stream_ops *php_stream_stdio_ops_with_offset = NULL; //todo: free
/*
*	micro_stream_open_function - zend_stream_open_function hooker
*	 used to mask binary headers of self
*/
int micro_stream_open_function(const char *filename, zend_file_handle *handle){
	if(NULL == micro_zend_stream_open_function_orig){
		// this should never happend
		abort();
	}
	if(SUCCESS != micro_zend_stream_open_function_orig(filename, handle)){
		return FAILURE;
	}
	if(0 == strcmp(filename, micro_get_filename())){
        if(ZEND_HANDLE_STREAM == handle->type){
            // it's a php stream
            dbgprintf("hooking zend_stream type php_stream with offset");
            // seek it to begin of payload first
            php_stream * ps = (handle->handle).stream.handle;
            if(NULL == php_stream_stdio_ops_with_offset){
                malloc(sizeof(*php_stream_stdio_ops_with_offset));
                memcpy(php_stream_stdio_ops_with_offset, &php_stream_stdio_ops, sizeof(php_stream_stdio_ops));
                php_stream_stdio_ops_with_offset->seek = micro_php_stream_seek_with_offset;
            }
            // assign ps->ops with offsetted ops
            ps->ops = php_stream_stdio_ops_with_offset;
            dbgprintf("initial seeking");
            zend_off_t dummy;
            ps->ops->seek(ps, 0, SEEK_SET, &dummy);
            // convert it to micro_stream_with_offset_t
            micro_stream_with_offset_t * ostream = malloc(sizeof(*ostream));
            *ostream = (micro_stream_with_offset_t){
                .reader_orig = handle->handle.stream.reader,
                .fsizer_orig = handle->handle.stream.fsizer,
                .closer_orig = handle->handle.stream.closer,
                .stream_orig = ps,
            };
            (handle->handle).stream = (zend_stream){
                .handle = ostream,
                .isatty = (handle->handle).stream.isatty,
                .reader = micro_zend_stream_reader,
                .fsizer = micro_zend_stream_fsizer_with_offset,
                .closer = micro_zend_stream_closer,
            };
        }else if(ZEND_HANDLE_FP == handle->type){
            // it's a fp
            dbgprintf("hooking zend_stream with offset");
            // seek it to begin of payload first
            FILE * fp = (handle->handle).stream.handle;
            fseek(fp, micro_get_sfx_filesize(), SEEK_SET);
            // convert it to micro_stream_fp_with_offset_t
            micro_stream_fp_with_offset_t * ostream = malloc(sizeof(*ostream));
            *ostream = (micro_stream_fp_with_offset_t){
                .reader_orig = handle->handle.stream.reader,
                .fsizer_orig = handle->handle.stream.fsizer,
                .closer_orig = handle->handle.stream.closer,
                .fp_orig = fp,
            };
            (handle->handle).stream = (zend_stream){
                .handle = ostream,
                .isatty = (handle->handle).stream.isatty,
                .reader = micro_zend_stream_fp_reader,
                .fsizer = micro_zend_stream_fp_fsizer_with_offset,
                .closer = micro_zend_stream_fp_closer,
            };
        }
    }
    return SUCCESS;
}

/*
*	micro_hook_zend_stream_ops - hook zend_stream_open_function ...
*/
int micro_hook_zend_stream_ops(void){
	if(NULL == zend_stream_open_function){
        // expect zend_stream_open_function to be php_stream_open_for_zend
		// refuse to hook before zend_stream_xx_function is setted
		return FAILURE;
	}
	micro_zend_stream_open_function_orig = zend_stream_open_function;
	return SUCCESS;
}