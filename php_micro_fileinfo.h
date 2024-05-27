/*
micro SAPI for PHP - php_micro_filesize.h
filesize reproduction utilities for micro

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

#ifndef _PHP_MICRO_FILESIZE_H
#define _PHP_MICRO_FILESIZE_H

#include <stdint.h>

#include "php.h"

uint32_t micro_get_sfxsize(void);

#ifdef PHP_WIN32
/*
 *   micro_get_filename_w - get self filename abs path (widechar)
 */
const wchar_t *micro_get_filename_w(void);
#endif
/*
 *   micro_get_filename - get self filename abs path (char *)
 */
const char *micro_get_filename(void);
/*
 *   micro_get_filename_len - get self filename abs path (char *) length
 */
size_t micro_get_filename_len(void);
/*
 *   micro_get_filename_slashed - get self filename with s/\\/\//g
 */
extern const char *(*micro_get_filename_slashed)(void);

extern struct _ext_ini {
    size_t size;
    char *data;
} micro_ext_ini;
/*
 *   micro_fileinfo_init - prepare micro_ext_ini for ext ini support
 */
int micro_fileinfo_init(void);

// things for phar hook (deprecated)

#ifdef MICRO_USE_OLD_PHAR_HOOK
/*
 *   is_stream_self - check if a phpstream is opened self
 */
int is_stream_self(php_stream *stream);
/*
 *   micro_php_stream_rewind - rewind a stream with offset
 */
#    define micro_php_stream_rewind(stream) \
        (is_stream_self(stream) ? _php_stream_seek(stream, micro_get_sfxsize(), SEEK_SET) \
                                : _php_stream_seek(stream, 0, SEEK_SET))
/*
 *   micro_php_stream_seek - seek a stream with offset
 */
#    define micro_php_stream_seek(stream, offset, whence) \
        (is_stream_self(stream) && SEEK_SET == whence ? dbgprintf("seeking with offset\n"), \
            _php_stream_seek(stream, offset + micro_get_sfxsize(), SEEK_SET) \
                                                      : _php_stream_seek(stream, 0, SEEK_SET))
#endif
/*
 *   zif_micro_get_sfx_filesize
 *	micro_get_sfx_filesize() -> int
 * 	get sfx size in bytes
 *     deprecated
 */
PHPAPI PHP_FUNCTION(micro_get_sfx_filesize);

/*
 *   zif_micro_get_sfxsize
 *	micro_get_sfxsize() -> int
 * 	get sfx size in bytes
 */
PHPAPI PHP_FUNCTION(micro_get_sfxsize);

/*
 *   zif_micro_get_self_filename
 *	micro_get_self_filename() -> string
 * 	get self absolute file path
 */
PHPAPI PHP_FUNCTION(micro_get_self_filename);

#endif
