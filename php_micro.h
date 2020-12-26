/*
micro SAPI for PHP - php_micro.h
header for micro

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
#ifndef _PHP_MICRO_H
#define _PHP_MICRO_H

#define PHP_MICRO_SFX_FILESIZE_ID 12345

#ifdef PHP_WIN32
# define MICRO_SFX_EXPORT __declspec(dllexport) __declspec(noinline)
#else
# define MICRO_SFX_EXPORT __attribute__((visibility ("default")))
#endif


#endif // _PHP_MICRO_H
