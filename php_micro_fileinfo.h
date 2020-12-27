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

int is_stream_self(php_stream * stream);
#define micro_php_stream_rewind(stream) (\
	is_stream_self(stream)?\
	_php_stream_seek(stream, micro_get_sfx_filesize(), SEEK_SET):\
    _php_stream_seek(stream, 0, SEEK_SET)\
)

#define micro_php_stream_seek(stream, offset, whence) (\
	is_stream_self(stream) && SEEK_SET == whence ?\
    dbgprintf("seeking with offset\n"),\
    _php_stream_seek(stream, offset + micro_get_sfx_filesize(), SEEK_SET):\
    _php_stream_seek(stream, 0, SEEK_SET)\
)

PHPAPI PHP_FUNCTION(micro_get_sfx_filesize);
PHPAPI PHP_FUNCTION(micro_get_self_filename);

#endif
