/*
micro SAPI for PHP - php_micro_filesize.h
filesize reproduction utilities for micro

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

#ifndef _PHP_MICRO_FILESIZE_H
#define _PHP_MICRO_FILESIZE_H

#include <stdint.h>

#include "php.h"

#include "php_micro_helper.h"

uint32_t micro_get_sfx_filesize();

const wchar_t * micro_get_filename_w();
const char * micro_get_filename();

zend_always_inline int is_stream_self(php_stream * stream){
	dbgprintf("checking %s\n", stream->orig_path);
#ifdef PHP_WIN32
	LPCWSTR stream_path_w = php_win32_ioutil_any_to_w(stream->orig_path);
	size_t stream_path_w_len = wcslen(stream_path_w);
	LPCWSTR my_path_w = micro_get_filename_w();
	size_t my_path_w_len = wcslen(my_path_w);
	dbgprintf("with self: %S\n", my_path_w);
	if (my_path_w_len == stream_path_w_len && 0 == wcscmp(stream_path_w, my_path_w)){
#else
	char* stream_path = stream->orig_path;
	size_t stream_path_len = strlen(stream_path);
	char* my_path = micro_get_filename();
	size_t my_path_len = strlen(my_path);
	dbgprintf("with self: %s\n", my_path);
	if (my_path_len == stream_path_len && 0 == wcscmp(stream_path, my_path)){
#endif
		dbgprintf("is self\n");
		return 1;
	}
	dbgprintf("not self\n");
	return 0;
}

zend_always_inline int micro_php_stream_rewind(php_stream * stream) {
	if(is_stream_self(stream)){
		return _php_stream_seek(stream, micro_get_sfx_filesize(), SEEK_SET);
	}
	return _php_stream_seek(stream, 0, SEEK_SET);
}

zend_always_inline int micro_php_stream_seek(php_stream * stream, int offset, int whence) {
	if(is_stream_self(stream) && SEEK_SET == whence){
        dbgprintf("seeking with offset\n");
		return _php_stream_seek(stream, offset + micro_get_sfx_filesize(), SEEK_SET);
	}
	return _php_stream_seek(stream, 0, SEEK_SET);
}

PHPAPI PHP_FUNCTION(micro_get_sfx_filesize);
PHPAPI PHP_FUNCTION(micro_get_self_filename);

#endif