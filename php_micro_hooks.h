/*
micro SAPI for PHP - php_micro_hooks.h
micro hooks for multi kinds of hooking header

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
#ifndef _PHP_MICRO_HOOKS_H
#define _PHP_MICRO_HOOKS_H

int micro_register_post_startup_cb(void);
int micro_hook_plain_files_wops(void);
int micro_reregister_proto(const char* proto);
#endif