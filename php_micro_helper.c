
#include <stdint.h>

#include "php.h"
//#include "ext/ffi/php_ffi.h"

#include "php_micro.h"

#ifdef PHP_WIN32

# include <windows.h>
# include <psapi.h>

HANDLE hOut, hErr;

/*
*   micro_init - prepare hErr and hOut for _myprintf
*/
int micro_init(void){
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if(INVALID_HANDLE_VALUE == hOut){
        wprintf("failed get output handle\n");
        return ENOMEM;
    }
    hErr = GetStdHandle(STD_ERROR_HANDLE);
    if(INVALID_HANDLE_VALUE == hErr){
        wprintf("failed get err handle\n");
        return ENOMEM;
    }
    return 0;
}

/*
*   mysprintf - a swprintf(3) like implemention for windows
*/
//#ifdef _DEBUG
//__declspec(dllexport) __declspec(noinline)
//#endif
wchar_t * mysprintf(const wchar_t * fmt, ...){
    LPVOID pBuf = NULL;
    va_list args = NULL;
    va_start(args, fmt);

    DWORD lenWords = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_STRING,
        fmt,
        0,
        0,
        (LPWSTR)&pBuf,
        0,
        &args
    );
    va_end(args);

    return pBuf;
}

MICRO_SFX_EXPORT int micro_format_output_w(const wchar_t * fmt, ...){
    LPVOID pBuf = NULL;
    va_list args = NULL;
    va_start(args, fmt);

    DWORD lenWords = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_STRING,
        fmt,
        0,
        0,
        (LPWSTR)&pBuf,
        0,
        &args
    );
    va_end(args);

    WriteConsoleW(hOut, pBuf, lenWords, &lenWords, 0);

    LocalFree(pBuf);
    return lenWords;
}

PHP_FUNCTION(micro_enum_modules){
    HMODULE hMods[1024];
    HANDLE hProcess = GetCurrentProcess();
    DWORD cbNeeded = 0;

    if(EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)){
        for(int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++){
            WCHAR szModName[MAX_PATH];

            // Get the full path to the module's file.

            if (GetModuleFileNameExW(
                hProcess,
                hMods[i],
                szModName,
                sizeof(szModName) / sizeof(WCHAR))){
                // Print the module name and handle value.

                printf("loaded: %S (%p)\n", szModName, hMods[i]);
            }
        }
    }
    RETURN_TRUE;
}
#endif // PHP_WIN32


#ifdef _DEBUG
// debug use
int dbgprintf(/*const char * fmt, ...*/){
    va_list args = NULL;
    va_start(args, fmt);
    //_setmode( _fileno( stdout ), _O_U16TEXT );
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}

PHP_FUNCTION(micro_update_extension_dir){
    char *new_dir;
	size_t new_dir_len;
    static int called = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(new_dir, new_dir_len)
	ZEND_PARSE_PARAMETERS_END();

    dbgprintf("updating %s as extension_dir\n", new_dir);

    if(!called){
        // first call here
        called = 1;
    }else{
        free(PG(extension_dir));
    }
    PG(extension_dir) = strdup(new_dir);

    dbgprintf("now is %s\n", PG(extension_dir));
    RETURN_TRUE;
}
MICRO_SFX_EXPORT int fuckcall(int(* func)(int), int input){
    printf("call %p with arg %d\n", func, input);
    int ret = func(input);
    printf("func(%d) = %d\n", input, ret);
    return ret;
}

MICRO_SFX_EXPORT void inspect(void* buf, int len){
    uint8_t * pbuf = buf;
    for(uint32_t i=0;i<len;i+=8){
        printf("%04x:", i);
        for(uint8_t j=0; i+j<len && j<8; j++){
            printf(" %02x", pbuf[i+j]);
        }
        printf("    ");
        for(uint8_t j=0; i+j<len && j<8; j++){
            if(' '<=pbuf[i+j] && pbuf[i+j]<127){
                printf("%c", pbuf[i+j]);
            }else{
                printf(".");
            }
        }
        printf("\n");
    }
}

MICRO_SFX_EXPORT void miaomiaomiao(void){
#define test(x) printf("match \"" x "\" : %d\n", match_format(L ## x, sizeof(x)));
    //test("%d!!");
    //test("%-11.012lld!!");
    //test("%%!!!");
    //test("%!!!");
    //myprintf(L"测试\n");
}
#endif // _DEBUG

