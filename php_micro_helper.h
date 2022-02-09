/*
micro SAPI for PHP - php_micro_helper.h
header for micro helpers

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
#ifndef _PHP_MICRO_DLHELPER_H
#define _PHP_MICRO_DLHELPER_H

#include "php.h"
#include "php_micro.h"

PHP_FUNCTION(micro_version);
/*
 *   micro_helper_init - prepare hErr and hOut for _myprintf
 */
int micro_helper_init(void);
/*
 *   micro_open_self_stream - prepare self stream without offset for php use
 */
// int micro_open_self_stream(void);

/*
 *   zif_micro_update_extension_dir
 *   micro_update_extension_dir(newdir) -> bool
 *   force add extension_dir out of ini
 *   only for debug in windows
 */
PHP_FUNCTION(micro_update_extension_dir);
/*
 *   zif_micro_enum_modules
 *   micro_enum_modules() -> bool
 *   show current loaded modules(dll)
 *   only for debug in windows
 */
PHP_FUNCTION(micro_enum_modules);
/*
 *   zif_micro_version
 *   micro_version() -> array
 *   get micro version
 *   in array():
 *       [ <major version>, <minor version>, <patch version>, [append version]]
 *   which <major version>, <minor version>, <patch version> is type of int,
 *   if append version defined, append version will be string(value of PHP_MICRO_VER_APP), otherwise array length will
 * be 3
 */
PHP_FUNCTION(micro_version);
/*
 *   zif_micro_open_self
 *   micro_open_self() -> mixed
 *   return self php_stream handle as php resource, if it's already closed, return false
 */
PHP_FUNCTION(micro_open_self);

#endif
