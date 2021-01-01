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

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

#define PHP_MICRO_VER_MAJ 0
#define PHP_MICRO_VER_MIN 0
#define PHP_MICRO_VER_PAT 1
#define PHP_MICRO_VER_APP "prealpha"
#ifdef PHP_MICRO_VER_APP
#define PHP_MICRO_VER_STR STRINGIZE(PHP_MICRO_VER_MAJ) "." STRINGIZE(PHP_MICRO_VER_MIN) "." STRINGIZE(PHP_MICRO_VER_PAT) "-" PHP_MICRO_VER_APP
#else
#define PHP_MICRO_VER_STR STRINGIZE(PHP_MICRO_VER_MAJ) "." STRINGIZE(PHP_MICRO_VER_MIN) "." STRINGIZE(PHP_MICRO_VER_PAT)
#endif

#define PHP_MICRO_SFX_FILESIZE_ID 12345
#ifdef PHP_WIN32
#define PHP_MICRO_HINT_CMDC "copy /b %s + mycode.php mycode.exe"
#define PHP_MICRO_HINT_CMDE "mycode.exe myarg1 myarg2"
#else
#define PHP_MICRO_HINT_CMDC "cat %s mycode.php > mycode"
#define PHP_MICRO_HINT_CMDE "./mycode myarg1 myarg2"
#endif
#define PHP_MICRO_HINT "micro SAPI for PHP v" PHP_MICRO_VER_STR "\n" \
    "Usage: concatenate this binary with any php code then execute it.\n" \
    "for example: if we have code as mycode.php, to concatenate them, execute:\n" \
    "    " PHP_MICRO_HINT_CMDC "\n" \
    "then execute it:\n" \
    "    " PHP_MICRO_HINT_CMDE "\n"

#ifdef PHP_WIN32
# define MICRO_SFX_EXPORT __declspec(dllexport) __declspec(noinline)
#else
# define MICRO_SFX_EXPORT __attribute__((visibility ("default")))
#endif

#define PHP_MICRO_INIMARK ((uint8_t[4]) {0xfd, 0xf6, 0x69, 0xe6})
#define PHP_MICRO_INIENTRY(x) ("micro." #x)

static inline const char *micro_slashize(const char *x){
    size_t size = strlen(x) + 1;
    char * ret = malloc(size);
    memcpy(ret, x, size);
    for(size_t i = 0; i<size - 1; i++){
        if('\\' == ret[i]){
            ret[i] = '/';
        }
    }
    return ret;
}

#endif // _PHP_MICRO_H
