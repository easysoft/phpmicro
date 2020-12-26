/*
micro SAPI for PHP - php_micro_filesize.c
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

#include "php.h"

#include "php_micro.h"
#include "php_micro_helper.h"

#include <stdint.h>
#if defined(PHP_WIN32)
#   include "win32/codepage.h"
#   include <windows.h>
#   define SFX_FILESIZE 0L
#elif defined(__linux)
#   include <sys/auxv.h>
#elif defined(__APPLE__)
#   include <mach-o/dyld.h>
#else
#error because we donot support that platform yet
#endif

uint32_t micro_get_sfx_filesize(){
    static uint32_t _sfx_filesize = SFX_FILESIZE;
#ifdef PHP_WIN32
    dbgprintf("_sfx_filesize: %d, %p\n",_sfx_filesize, &_sfx_filesize);
    dbgprintf("resource: %p\n",FindResourceA(NULL, MAKEINTRESOURCEA(PHP_MICRO_SFX_FILESIZE_ID), RT_RCDATA));
    dbgprintf("err: %8x\n", GetLastError());
    if(SFX_FILESIZE == _sfx_filesize){
        memcpy(
            (void *)&_sfx_filesize,
            LockResource(
                LoadResource(
                    NULL,
                    FindResourceA(
                        NULL,
                        MAKEINTRESOURCEA(PHP_MICRO_SFX_FILESIZE_ID),
                        RT_RCDATA
                    )
                )
            ),
            sizeof(uint32_t)
        );
    }
    return _sfx_filesize;
#else
    return _sfx_filesize;
#endif
}

#ifdef PHP_WIN32
const wchar_t* micro_get_filename_w(){
    static LPWSTR self_filename = NULL;
    //dbgprintf("fuck %S\n", self_filename);
    if(self_filename){
        return self_filename;
    }
    DWORD self_filename_chars = MAX_PATH;
    self_filename = malloc(self_filename_chars * sizeof(WCHAR));
    DWORD wapiret = 0;
    
    while(self_filename_chars == (wapiret = GetModuleFileNameW(NULL, self_filename, self_filename_chars))){
        dbgprintf("wapiret is %d\n", wapiret);
        dbgprintf("lensize is %d\n", self_filename_chars);
        if(
#   if WINVER < _WIN32_WINNT_VISTA
            ERROR_SUCCESS 
#   else
            ERROR_INSUFFICIENT_BUFFER
#   endif
            == GetLastError()
        ){
            self_filename_chars += MAX_PATH;
            self_filename = realloc(self_filename, self_filename_chars * sizeof(WCHAR));
        }else{
            dbgprintf("cannot get self path\n");
            return NULL;
        }
    };
    dbgprintf("wapiret is %d\n", wapiret);
    dbgprintf("lensize is %d\n", self_filename_chars);


    if(wapiret > MAX_PATH && memcmp(L"\\\\?\\", self_filename, 4*sizeof(WCHAR))){
        dbgprintf("\\\\?\\-ize self_filename\n");
        LPWSTR buf = malloc((wapiret + 5)*sizeof(WCHAR));
        memcpy(buf, L"\\\\?\\", 4*sizeof(WCHAR));
        memcpy(buf+4, self_filename, wapiret*sizeof(WCHAR));
        buf[wapiret + 4] = L'\0';
        free(self_filename);
        self_filename = buf;
    }

    dbgprintf("self is %S\n", self_filename);

    return self_filename;
}

const char * micro_get_filename(){
    return php_win32_cp_w_to_utf8(micro_get_filename_w());
}
#elif defined(__linux)
const char * micro_get_filename(){
    return (char*)getauxval(AT_EXECFN);
}
#elif defined(__APPLE__)
const char * micro_get_filename(){
    static const char nullstr[1] = "";
    static char * self_path = NULL;
    if (NULL == self_path){
        uint32_t len = 0;
        if (-1 != _NSGetExecutablePath(NULL, &len)) {
            goto error;
        }
        self_path = malloc(len);
        if (NULL == self_path) {
            goto error;
        }
        if (0 != _NSGetExecutablePath(self_path, &len)) {
            goto error;
        }

    }
    return self_path;
    error:
    self_path = nullstr;
    return NULL;
}
#endif

PHP_FUNCTION(micro_get_self_filename){
    RETURN_STRING(micro_get_filename());
}

PHP_FUNCTION(micro_get_sfx_filesize){
    RETURN_LONG(micro_get_sfx_filesize());
}
