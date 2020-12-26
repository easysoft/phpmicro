/*
micro SAPI for PHP - php_micro_helper.h
header for micro helpers

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
#ifndef _PHP_MICRO_DLHELPER_H
#define _PHP_MICRO_DLHELPER_H

#include "php.h"
#include "php_micro.h"

#ifdef _DEBUG
int micro_init(void);
MICRO_SFX_EXPORT int dbgprintf(const char * fmt, ...);
#else
#define dbgprintf(...)
#endif

PHP_FUNCTION(micro_update_extension_dir);
PHP_FUNCTION(micro_enum_modules);
#endif
