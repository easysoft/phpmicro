/*
micro SAPI for PHP - php_micro_helper.h
micro helpers like dbgprintf

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
#include <stdint.h>

#include "php.h"

#include "php_micro.h"
#include "php_micro_fileinfo.h"

#if defined(PHP_WIN32) && defined(_DEBUG)

#    include <psapi.h>
#    include <windows.h>

HANDLE hOut, hErr;

int micro_helper_init(void) {
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (INVALID_HANDLE_VALUE == hOut) {
        wprintf(L"failed get output handle\n");
        return ENOMEM;
    }
    hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (INVALID_HANDLE_VALUE == hErr) {
        wprintf(L"failed get err handle\n");
        return ENOMEM;
    }
    return 0;
}

/*
 *   micro_sprintf - a swprintf(3) like implemention for windows
 *   only for debug in ffi calling procedure
 */
MICRO_SFX_EXPORT wchar_t *micro_sprintf(const wchar_t *fmt, ...) {
    LPVOID pBuf = NULL;
    va_list args;
    va_start(args, fmt);

    DWORD lenWords =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING, fmt, 0, 0, (LPWSTR)&pBuf, 0, &args);
    va_end(args);

    return pBuf;
}

/*
 *   micro_wprintf - a wprintf(3) like implemention for windows
 *   only for debug in ffi calling procedure
 */
MICRO_SFX_EXPORT int micro_wprintf(const wchar_t *fmt, ...) {
    LPVOID pBuf = NULL;
    va_list args;
    va_start(args, fmt);

    DWORD lenWords =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING, fmt, 0, 0, (LPWSTR)&pBuf, 0, &args);
    va_end(args);

    WriteConsoleW(hOut, pBuf, lenWords, &lenWords, 0);

    LocalFree(pBuf);
    return lenWords;
}

PHP_FUNCTION(micro_enum_modules) {
    HMODULE hMods[1024];
    HANDLE hProcess = GetCurrentProcess();
    DWORD cbNeeded = 0;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            WCHAR szModName[MAX_PATH];

            // Get the full path to the module's file.

            if (GetModuleFileNameExW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(WCHAR))) {
                // Print the module name and handle value.

                printf("loaded: %S (%p)\n", szModName, hMods[i]);
            }
        }
    }
    RETURN_TRUE;
}

#endif // defined(PHP_WIN32) && defined(_DEBUG)

#ifdef _DEBUG
PHP_FUNCTION(micro_update_extension_dir) {
    char *new_dir;
    size_t new_dir_len;
    static int called = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(new_dir, new_dir_len)
    ZEND_PARSE_PARAMETERS_END();

    dbgprintf("updating %s as extension_dir\n", new_dir);

    if (!called) {
        // first call here
        called = 1;
    } else {
        free(PG(extension_dir));
    }
    PG(extension_dir) = strdup(new_dir);

    dbgprintf("now is %s\n", PG(extension_dir));
    RETURN_TRUE;
}

// ffi debug functions here

MICRO_SFX_EXPORT int testcall(int (*func)(int), int input) {
    printf("call %p with arg %d\n", func, input);
    int ret = func(input);
    printf("func(%d) = %d\n", input, ret);
    return ret;
}

MICRO_SFX_EXPORT void inspect(void *buf, int len) {
    uint8_t *pbuf = buf;
    for (uint32_t i = 0; i < len; i += 8) {
        printf("%04x:", i);
        for (uint8_t j = 0; i + j < len && j < 8; j++) { printf(" %02x", pbuf[i + j]); }
        printf("    ");
        for (uint8_t j = 0; i + j < len && j < 8; j++) {
            if (' ' <= pbuf[i + j] && pbuf[i + j] < 127) {
                printf("%c", pbuf[i + j]);
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
}

MICRO_SFX_EXPORT void emptyfunc(void) {
}

#endif //_DEBUG

PHP_FUNCTION(micro_version) {
    array_init(return_value);
    zval zv;
    ZVAL_LONG(&zv, PHP_MICRO_VER_MAJ);
    zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &zv);
    ZVAL_LONG(&zv, PHP_MICRO_VER_MIN);
    zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &zv);
    ZVAL_LONG(&zv, PHP_MICRO_VER_PAT);
    zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &zv);
#ifdef PHP_MICRO_VER_APP
    ZVAL_STRING(&zv, PHP_MICRO_VER_APP);
    zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &zv);
#endif
}

PHP_FUNCTION(micro_open_self) {
    php_stream *stream = NULL;
    FILE *fp = VCWD_FOPEN(micro_get_filename(), "rb");
    stream = php_stream_fopen_from_file(fp, "rb");
    if (NULL == stream) {
        RETURN_FALSE;
    }
    php_stream_to_zval(stream, return_value);
}

#ifdef PHP_WIN32

// for windows "win32" build to reallocate console
PHP_FUNCTION(realloc_console) {
    BOOL ret = 0;
    zend_bool alloc = false;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(alloc)
    ZEND_PARSE_PARAMETERS_END();

    if (alloc) {
        ret = AllocConsole();
    } else {
        ret = AttachConsole(ATTACH_PARENT_PROCESS);
    }

    if (ret) {
        _close(0);
        _close(1);
        _close(2);
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", __acrt_iob_func(1));
        freopen("CONOUT$", "w", __acrt_iob_func(2));
    }

    RETURN_BOOL(ret);
}

# endif // PHP_WIN32