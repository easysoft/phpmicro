/*
micro SAPI for PHP - php_micro_filesize.c
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

#include "php.h"

#include "php_micro.h"
#include "php_micro_helper.h"

#include <stdint.h>
#if defined(PHP_WIN32)
#    include "win32/codepage.h"
#    include <windows.h>
#    define SFX_FILESIZE 0L
#elif defined(__linux)
#    include <sys/auxv.h>
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#else
#    error because we donot support that platform yet
#endif

#ifndef PHP_WIN32
#    include <errno.h>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif // ndef PHP_WIN32

const char *micro_get_filename(void);

// do we need uint64_t for sfx size?
static uint32_t _final_sfx_filesize = 0;
uint32_t _micro_get_sfx_filesize(void);
uint32_t micro_get_sfx_filesize(void) {
    return _final_sfx_filesize;
}

typedef struct _ext_ini_header_t {
    uint8_t magic[4];
    uint8_t len[4];
} ext_ini_header_t;

struct _ext_ini {
    size_t size;
    char *data;
} micro_ext_ini = {.size = 0, .data = NULL};

// shabby endian-independent check
#define checkmagic(var) \
    (var[0] != PHP_MICRO_INIMARK[0] || var[1] != PHP_MICRO_INIMARK[1] || var[2] != PHP_MICRO_INIMARK[2] || \
        var[3] != PHP_MICRO_INIMARK[3])

#ifdef PHP_WIN32
const wchar_t *micro_get_filename_w();
#endif // PHP_WIN32

int micro_fileinfo_init(void) {
    int ret = 0;
    uint32_t len = 0;
    uint32_t sfx_filesize = _micro_get_sfx_filesize();
#ifdef PHP_WIN32
    LPCWSTR self_path = micro_get_filename_w();
    HANDLE handle = CreateFileW(self_path,
        FILE_ATTRIBUTE_READONLY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (INVALID_HANDLE_VALUE == handle) {
        ret = FAILURE;
        goto end;
    }
    DWORD filesize = GetFileSize(handle, NULL);
    dbgprintf("%d, %d\n", sfx_filesize, filesize);
    if (filesize <= sfx_filesize) {
        fwprintf(stderr, L"no payload found.\n" PHP_MICRO_HINT, self_path);
        ret = FAILURE;
        goto end;
    }
#    define seekfile(x) \
        do { SetFilePointer(handle, x, 0, FILE_BEGIN); } while (0)
#    define readfile(dest, size, red) \
        do { ReadFile(handle, dest, size, &red, NULL); } while (0)
#    define closefile() \
        do { \
            if (INVALID_HANDLE_VALUE != handle) { \
                CloseHandle(handle); \
            } \
        } while (0)
#else
    const char *self_path = micro_get_filename();
    int fd = open(self_path, O_RDONLY);
    if (-1 == fd) {
        // TODO: tell failed here
        ret = errno;
        goto end;
    }
    struct stat stats;
    ret = stat(self_path, &stats);
    if (-1 == ret) {
        // TODO: tell failed here
        ret = errno;
        goto end;
    }
    size_t filesize = stats.st_size;
    if (filesize <= sfx_filesize) {
        fprintf(stderr, "no payload found.\n" PHP_MICRO_HINT, self_path);
        ret = FAILURE;
        goto end;
    }
#    define seekfile(x) \
        do { lseek(fd, x, SEEK_SET); } while (0)
#    define readfile(dest, size, red) \
        do { red = read(fd, dest, size); } while (0)
#    define closefile() \
        do { \
            if (-1 != fd) { \
                close(fd); \
            } \
        } while (0)
#endif // PHP_WIN32
    ext_ini_header_t ext_ini_header = {0};
    if (filesize <= sfx_filesize + sizeof(ext_ini_header)) {
        ret = FAILURE;
        goto end;
    }
    // we may have extra ini configs.
    seekfile(sfx_filesize);
    uint32_t red = 0;
    readfile(&ext_ini_header, sizeof(ext_ini_header), red);
    if (sizeof(ext_ini_header) != red) {
        // cannot read file
        ret = errno;
        goto end;
    }

    if (checkmagic(ext_ini_header.magic)) {
        // bad magic, not an extra ini
        ret = SUCCESS;
        goto end;
    }
    // shabby ntohl
    len = (ext_ini_header.len[0] << 24) + (ext_ini_header.len[1] << 16) + (ext_ini_header.len[2] << 8) +
        ext_ini_header.len[3];
    dbgprintf("len is %d\n", len);
    if (filesize <= sfx_filesize + sizeof(ext_ini_header) + len) {
        // bad len, not an extra ini
        ret = SUCCESS;
        len = 0;
        goto end;
    }
    micro_ext_ini.data = malloc(len + 2);
    readfile(micro_ext_ini.data, len, red);
    if (len != red) {
        // cannot read file
        ret = errno;
        len = 0;
        free(micro_ext_ini.data);
        micro_ext_ini.data = NULL;
        goto end;
    }
    // two '\0's like hardcoden inis
    micro_ext_ini.data[len] = '\0';
    micro_ext_ini.data[len + 1] = '\0';
    dbgprintf("using ext ini %s\n", micro_ext_ini.data);
    micro_ext_ini.size = len + 1;
    len += sizeof(ext_ini_header_t);

end:
    _final_sfx_filesize = sfx_filesize + len;
    closefile();
    return ret;
#undef seekfile
#undef readfile
#undef closefile
}

/*
 *   _micro_get_sfx_filesize - get (real) sfx size using resource(win) / 2 stage build constant (others)
 */
uint32_t _micro_get_sfx_filesize(void) {
    static uint32_t _sfx_filesize = SFX_FILESIZE;
#ifdef PHP_WIN32
    dbgprintf("_sfx_filesize: %d, %p\n", _sfx_filesize, &_sfx_filesize);
    dbgprintf("resource: %p\n", FindResourceA(NULL, MAKEINTRESOURCEA(PHP_MICRO_SFX_FILESIZE_ID), RT_RCDATA));
    dbgprintf("err: %8x\n", GetLastError());
    if (SFX_FILESIZE == _sfx_filesize) {
        memcpy((void *)&_sfx_filesize,
            LockResource(
                LoadResource(NULL, FindResourceA(NULL, MAKEINTRESOURCEA(PHP_MICRO_SFX_FILESIZE_ID), RT_RCDATA))),
            sizeof(uint32_t));
    }
    return _sfx_filesize;
#else
    return _sfx_filesize;
#endif
}

#ifdef PHP_WIN32

const wchar_t *micro_get_filename_w() {
    static LPWSTR self_filename = NULL;
    // dbgprintf("fuck %S\n", self_filename);
    if (self_filename) {
        return self_filename;
    }
    DWORD self_filename_chars = MAX_PATH;
    self_filename = malloc(self_filename_chars * sizeof(WCHAR));
    DWORD wapiret = 0;

    DWORD (*myGetModuleFileNameExW)(HANDLE, HMODULE, LPWSTR, DWORD);

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    myGetModuleFileNameExW = (void *)GetProcAddress(hKernel32, "K32GetModuleFileNameExW");
    if (NULL == myGetModuleFileNameExW) {
        HMODULE hPsapi = GetModuleHandleW(L"psapi.dll");
        myGetModuleFileNameExW = (void *)GetProcAddress(hPsapi, "GetModuleFileNameExW");
    }
    if (NULL == myGetModuleFileNameExW) {
        dbgprintf("cannot get self path via win32api\n");
        return NULL;
    }
    while (self_filename_chars ==
        (wapiret = myGetModuleFileNameExW(GetCurrentProcess(), NULL, self_filename, self_filename_chars))) {
        dbgprintf("wapiret is %d\n", wapiret);
        dbgprintf("lensize is %d\n", self_filename_chars);
        if (
#    if WINVER < _WIN32_WINNT_VISTA
            ERROR_SUCCESS
#    else
            ERROR_INSUFFICIENT_BUFFER
#    endif
            == GetLastError()) {
            self_filename_chars += MAX_PATH;
            self_filename = realloc(self_filename, self_filename_chars * sizeof(WCHAR));
        } else {
            dbgprintf("cannot get self path\n");
            return NULL;
        }
    };
    dbgprintf("wapiret is %d\n", wapiret);
    dbgprintf("lensize is %d\n", self_filename_chars);

    if (wapiret > MAX_PATH && memcmp(L"\\\\?\\", self_filename, 4 * sizeof(WCHAR))) {
        dbgprintf("\\\\?\\-ize self_filename\n");
        LPWSTR buf = malloc((wapiret + 5) * sizeof(WCHAR));
        memcpy(buf, L"\\\\?\\", 4 * sizeof(WCHAR));
        memcpy(buf + 4, self_filename, wapiret * sizeof(WCHAR));
        buf[wapiret + 4] = L'\0';
        free(self_filename);
        self_filename = buf;
    }

    dbgprintf("self is %S\n", self_filename);

    return self_filename;
}

const char *micro_get_filename(void) {
    return php_win32_cp_w_to_utf8(micro_get_filename_w());
}

#elif defined(__linux)
const char *micro_get_filename(void) {
    static char *self_filename = NULL;
    if (NULL == self_filename) {
        self_filename = malloc(PATH_MAX);
        realpath((const char *)getauxval(AT_EXECFN), self_filename);
    }
    return self_filename;
}
#elif defined(__APPLE__)
const char *micro_get_filename(void) {
    static const char nullstr[1] = "";
    static char *self_path = NULL;
    if (NULL == self_path) {
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
#else
#    error "not support this system yet"
#endif

size_t micro_get_filename_len(void) {
    static size_t _micro_filename_l = -1;
    if (-1 == _micro_filename_l) {
        _micro_filename_l = strlen(micro_get_filename());
    }
    return _micro_filename_l;
}

// deprecated
#if MICRO_USE_OLD_PHAR_HOOK
int is_stream_self(php_stream *stream) {
    dbgprintf("checking %s\n", stream->orig_path);
#    ifdef PHP_WIN32
    LPCWSTR stream_path_w = php_win32_ioutil_any_to_w(stream->orig_path);
    size_t stream_path_w_len = wcslen(stream_path_w);
    LPCWSTR my_path_w = micro_get_filename_w();
    size_t my_path_w_len = wcslen(my_path_w);
    dbgprintf("with self: %S\n", my_path_w);
    if (my_path_w_len == stream_path_w_len && 0 == wcscmp(stream_path_w, my_path_w)) {
#    else
    const char *stream_path = stream->orig_path;
    size_t stream_path_len = strlen(stream_path);
    const char *my_path = micro_get_filename();
    size_t my_path_len = strlen(my_path);
    dbgprintf("with self: %s\n", my_path);
    if (my_path_len == stream_path_len && 0 == strcmp(stream_path, my_path)) {
#    endif
        dbgprintf("is self\n");
        return 1;
    }
    dbgprintf("not self\n");
    return 0;
}
#endif

PHP_FUNCTION(micro_get_self_filename) {
    RETURN_STRING(micro_get_filename());
}

PHP_FUNCTION(micro_get_sfx_filesize) {
    RETURN_LONG(micro_get_sfx_filesize());
}
