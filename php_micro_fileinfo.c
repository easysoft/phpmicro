
#include <stdint.h>
#include <sys/auxv.h>

#include "php.h"

#include "php_micro.h"
#include "php_micro_helper.h"

#ifdef PHP_WIN32
# include "win32/codepage.h"
# define SFX_FILESIZE 0L
#endif

uint32_t micro_get_sfx_filesize(){
    static volatile uint32_t _sfx_filesize = SFX_FILESIZE;
#ifdef PHP_WIN32
    if(0 == _sfx_filesize){
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
#else
const char * micro_get_filename(){
    return (char*)getauxval(AT_EXECFN);
}
#endif

PHP_FUNCTION(micro_get_self_filename){
    RETURN_STRING(micro_get_filename());
}

PHP_FUNCTION(micro_get_sfx_filesize){
    RETURN_LONG(micro_get_sfx_filesize());
}
