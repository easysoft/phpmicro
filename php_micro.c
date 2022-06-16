/*
micro SAPI for PHP - php_micro.c
main file for micro sapi

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

this file contains source from php
here's original copyright notice
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Edin Kadribasic <edink@php.net>                              |
   |         Marcus Boerger <helly@php.net>                               |
   |         Johannes Schlueter <johannes@php.net>                        |
   |         Parts based on CGI SAPI Module by                            |
   |         Rasmus Lerdorf, Stig Bakken and Zeev Suraski                 |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "php_globals.h"
#include "php_main.h"
#include "php_variables.h"

#ifdef PHP_WIN32
#    include "win32/console.h"
#    include "win32/ioutil.h"
#    include "win32/select.h"
#    include "win32/signal.h"
#    include "win32/time.h"
BOOL php_win32_init_random_bytes(void);
BOOL php_win32_shutdown_random_bytes(void);
BOOL php_win32_ioutil_init(void);
void php_win32_init_gettimeofday(void);
#else
#    define php_select(m, r, w, e, t) select(m, r, w, e, t)
#    include <fcntl.h>
#    include <sys/stat.h>
#endif

#include "ext/standard/php_standard.h"
#include "sapi/cli/php_cli_process_title.h"
#include "sapi/cli/ps_title.h"
#if PHP_VERSION_ID >= 80000
#    include "sapi/cli/php_cli_process_title_arginfo.h"
#endif

#include "SAPI.h"

#include "php_micro.h"
#include "php_micro_fileinfo.h"
#include "php_micro_helper.h"
#include "php_micro_hooks.h"

const char HARDCODED_INI[] = "html_errors=0\n"
                             "register_argc_argv=1\n"
                             "implicit_flush=1\n"
                             "output_buffering=0\n"
                             "max_execution_time=0\n"
                             "max_input_time=-1\n\0";

static char *php_self = "";
static char *script_filename = "";

static inline int sapi_micro_select(php_socket_t fd) {
    fd_set wfd;
    struct timeval tv;
    int ret;

    FD_ZERO(&wfd);

    PHP_SAFE_FD_SET(fd, &wfd);

    tv.tv_sec = (long)FG(default_socket_timeout);
    tv.tv_usec = 0;

    ret = php_select(fd + 1, NULL, &wfd, NULL, &tv);

    return ret != -1;
}

static void sapi_micro_register_variables(zval *track_vars_array) /* {{{ */
{
    size_t len;
    char *docroot = "";

    /* In CGI mode, we consider the environment to be a part of the server
     * variables
     */
    php_import_environment_variables(track_vars_array);

    /* Build the special-case PHP_SELF variable for the CLI version */
    len = strlen(php_self);
    if (sapi_module.input_filter(PARSE_SERVER, "PHP_SELF", &php_self, len, &len)) {
        php_register_variable("PHP_SELF", php_self, track_vars_array);
    }
    if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_NAME", &php_self, len, &len)) {
        php_register_variable("SCRIPT_NAME", php_self, track_vars_array);
    }
    /* filenames are empty for stdin */
    len = strlen(script_filename);
    if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_FILENAME", &script_filename, len, &len)) {
        php_register_variable("SCRIPT_FILENAME", script_filename, track_vars_array);
    }
    if (sapi_module.input_filter(PARSE_SERVER, "PATH_TRANSLATED", &script_filename, len, &len)) {
        php_register_variable("PATH_TRANSLATED", script_filename, track_vars_array);
    }
    /* just make it available */
    len = 0U;
    if (sapi_module.input_filter(PARSE_SERVER, "DOCUMENT_ROOT", &docroot, len, &len)) {
        php_register_variable("DOCUMENT_ROOT", docroot, track_vars_array);
    }
}

int micro_register_post_startup_cb(void);
static int php_micro_startup(sapi_module_struct *sapi_module) /* {{{ */
{
    /*
    zend_module_entry fake_module_entry = {
        STANDARD_MODULE_HEADER,
        "micro",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        PHP_VERSION,
        STANDARD_MODULE_PROPERTIES
    };
    */
    micro_register_post_startup_cb();

    if (
#if PHP_VERSION_ID >= 80200
        php_module_startup(sapi_module, NULL) == FAILURE
#else
        php_module_startup(sapi_module, NULL, 0) == FAILURE
#endif
    ) {
        return FAILURE;
    }
    return SUCCESS;
}

int php_micro_module_shutdown_wrapper(sapi_module_struct *sapi_globals) {
    php_module_shutdown();
    return SUCCESS;
}

static int sapi_micro_deactivate(void) /* {{{ */
{
    fflush(stdout);
    if (SG(request_info).argv0) {
        free(SG(request_info).argv0);
        SG(request_info).argv0 = NULL;
    }
    return SUCCESS;
}

static size_t sapi_micro_ub_write(const char *str, size_t str_length) /* {{{ */
{
    const char *ptr = str;
    size_t remaining = str_length;
    ssize_t ret;

    if (!str_length) {
        return 0;
    }
    /* todo: fix
        if (cli_shell_callbacks.cli_shell_ub_write) {
            size_t ub_wrote;
            ub_wrote = cli_shell_callbacks.cli_shell_ub_write(str, str_length);
            if (ub_wrote != (size_t) -1) {
                return ub_wrote;
            }
        }
    */

    while (remaining > 0) {
        /* TODO: fix
            if (cli_shell_callbacks.cli_shell_write) {
                cli_shell_callbacks.cli_shell_write(str, str_length);
            }
        */

#ifdef PHP_WRITE_STDOUT
        do {
            ret = write(STDOUT_FILENO, str, str_length);
        } while (ret <= 0 && errno == EAGAIN && sapi_micro_select(STDOUT_FILENO));
#else
        ret = fwrite(str, 1, MIN(str_length, 16384), stdout);
        if (ret == 0 && ferror(stdout)) {
            ret = -1;
        }
#endif
        if (ret < 0) {
#ifndef PHP_MICRO_WIN32_NO_CONSOLE
            EG(exit_status) = 255;
            php_handle_aborted_connection();
#endif
            break;
        }
        ptr += ret;
        remaining -= ret;
    }

    return (ptr - str);
}
/* }}} */

static void sapi_micro_flush(void *server_context) /* {{{ */
{
    /* Ignore EBADF here, it's caused by the fact that STDIN/STDOUT/STDERR streams
     * are/could be closed before fflush() is called.
     */
    if (fflush(stdout) == EOF && errno != EBADF) {
#ifndef PHP_MICRO_WIN32_NO_CONSOLE
        php_handle_aborted_connection();
#endif
    }
}
/* }}} */

static int sapi_micro_header_handler(sapi_header_struct *h, sapi_header_op_enum op, sapi_headers_struct *s) /* {{{ */
{
    return 0;
}
/* }}} */

static int sapi_micro_send_headers(sapi_headers_struct *sapi_headers) /* {{{ */
{
    /* We do nothing here, this function is needed to prevent that the fallback
     * header handling is called. */
    return SAPI_HEADER_SENT_SUCCESSFULLY;
}
/* }}} */

static void sapi_micro_send_header(sapi_header_struct *sapi_header, void *server_context) /* {{{ */
{
}
/* }}} */

static char *sapi_micro_read_cookies(void) /* {{{ */
{
    return NULL;
}
/* }}} */

static void sapi_micro_log_message(const char *message, int syslog_type_int) /* {{{ */
{
    fprintf(stderr, "%s\n", message);
#ifdef PHP_WIN32
    fflush(stderr);
#endif
}
/* }}} */

/* {{{ sapi_module_struct micro_sapi_module
 */
static sapi_module_struct micro_sapi_module = {
#ifdef PHP_MICRO_FAKE_CLI
    "cli", /* name */
#else
    "micro", /* name */
#endif
    "micro PHP sfx",                   /* pretty name */
    php_micro_startup,                 /* startup */
    php_micro_module_shutdown_wrapper, /* shutdown */
    NULL,                              /* activate */
    sapi_micro_deactivate,             /* deactivate */
    sapi_micro_ub_write,               /* unbuffered write */
    sapi_micro_flush,                  /* flush */
    NULL,                              /* get uid */
    NULL,                              /* getenv */
    php_error,                         /* error handler */
    sapi_micro_header_handler,         /* header handler */
    sapi_micro_send_headers,           /* send headers handler */
    sapi_micro_send_header,            /* send header handler */
    NULL,                              /* read POST data */
    sapi_micro_read_cookies,           /* read Cookies */
    sapi_micro_register_variables,     /* register server variables */
    sapi_micro_log_message,            /* Log message */
    NULL,                              /* Get request time */
    NULL,                              /* Child terminate */
    STANDARD_SAPI_MODULE_PROPERTIES,
};
/* }}} */

/*
 *	sapi_micro_ini_defaults - set overwriteable ini defaults
 */
static void sapi_micro_ini_defaults(HashTable *configuration_hash) {
    zval tmp;
/* overwriteable ini defaults must be set in sapi_micro_ini_defaults() */
#define INI_DEFAULT(name, value) \
    ZVAL_NEW_STR(&tmp, zend_string_init(value, sizeof(value) - 1, 1)); \
    zend_hash_str_update(configuration_hash, name, sizeof(name) - 1, &tmp);
    INI_DEFAULT("report_zend_debug", "0");
    INI_DEFAULT("display_errors", "1");
#undef INI_DEFAULT

    // omit PG(php_binary) before any php_ini codes used it
    if (NULL != PG(php_binary)) {
        free(PG(php_binary));
        PG(php_binary) = NULL;
    }
}

#ifdef _DEBUG

ZEND_BEGIN_ARG_INFO(arginfo_dl, 0)
    ZEND_ARG_INFO(0, extension_filename)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO(arginfo_micro_update_extension_dir, 0)
    ZEND_ARG_INFO(0, new_dir)
ZEND_END_ARG_INFO()

#    ifdef PHP_WIN32
ZEND_BEGIN_ARG_INFO(arginfo_micro_enum_modules, 0)
ZEND_END_ARG_INFO()

#    endif // PHP_WIN32

#endif

ZEND_BEGIN_ARG_INFO(arginfo_micro_get_sfx_filesize, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_micro_get_self_filename, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_micro_version, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_micro_open_self, 0)
ZEND_END_ARG_INFO()

// clang-format off
static const zend_function_entry additional_functions[] = {
#ifdef _DEBUG
    // debug functions
    ZEND_FE(dl, arginfo_dl)
    PHP_FE(micro_update_extension_dir, arginfo_micro_update_extension_dir)
#   ifdef PHP_WIN32
    PHP_FE(micro_enum_modules, arginfo_micro_enum_modules)
#   endif // PHP_WIN32
#endif // _DEBUG
    // cli functions
	PHP_FE(cli_set_process_title, arginfo_cli_set_process_title)
	PHP_FE(cli_get_process_title, arginfo_cli_get_process_title)
    // micro sapi functions
    PHP_FE(micro_get_sfx_filesize, arginfo_micro_get_sfx_filesize)
    PHP_FE(micro_get_self_filename, arginfo_micro_get_self_filename)
    PHP_FE(micro_version, arginfo_micro_version)
    PHP_FE(micro_open_self, arginfo_micro_open_self)
    PHP_FE_END
};
// clang-format on

// static php_stream *s_in_process = NULL;

static void micro_register_file_handles(void) /* {{{ */
{
    php_stream *s_in, *s_out, *s_err;
    php_stream_context *sc_in = NULL, *sc_out = NULL, *sc_err = NULL;
    zend_constant ic, oc, ec;

    s_in = php_stream_open_wrapper_ex("php://stdin", "rb", 0, NULL, sc_in);
    s_out = php_stream_open_wrapper_ex("php://stdout", "wb", 0, NULL, sc_out);
    s_err = php_stream_open_wrapper_ex("php://stderr", "wb", 0, NULL, sc_err);

    /* Release stream resources, but don't free the underlying handles. Othewrise,
     * extensions which write to stderr or company during mshutdown/gshutdown
     * won't have the expected functionality.
     */
    if (s_in)
        s_in->flags |= PHP_STREAM_FLAG_NO_CLOSE;
    if (s_out)
        s_out->flags |= PHP_STREAM_FLAG_NO_CLOSE;
    if (s_err)
        s_err->flags |= PHP_STREAM_FLAG_NO_CLOSE;

    if (s_in == NULL || s_out == NULL || s_err == NULL) {
        if (s_in)
            php_stream_close(s_in);
        if (s_out)
            php_stream_close(s_out);
        if (s_err)
            php_stream_close(s_err);
        return;
    }

    // s_in_process = s_in;

    php_stream_to_zval(s_in, &ic.value);
    php_stream_to_zval(s_out, &oc.value);
    php_stream_to_zval(s_err, &ec.value);

    ZEND_CONSTANT_SET_FLAGS(&ic, CONST_CS, 0);
    ic.name = zend_string_init_interned("STDIN", sizeof("STDIN") - 1, 0);
    zend_register_constant(&ic);

    ZEND_CONSTANT_SET_FLAGS(&oc, CONST_CS, 0);
    oc.name = zend_string_init_interned("STDOUT", sizeof("STDOUT") - 1, 0);
    zend_register_constant(&oc);

    ZEND_CONSTANT_SET_FLAGS(&ec, CONST_CS, 0);
    ec.name = zend_string_init_interned("STDERR", sizeof("STDERR") - 1, 0);
    zend_register_constant(&ec);
}
/* }}} */

#ifdef _DEBUG
int micro_debug = 0;
#endif
/* {{{ main
 */
#ifdef PHP_MICRO_WIN32_NO_CONSOLE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
    int exit_status = 0;
#ifdef _DEBUG
    const char *_debug_env = getenv("MICRO_DEBUG");
    micro_debug = _debug_env && (int)strnlen(_debug_env, 1);
#endif
#if defined(PHP_WIN32) && defined(_DEBUG)
    if (0 != (exit_status = micro_helper_init())) {
        return exit_status;
    }
#endif
    if (0 != (exit_status = micro_fileinfo_init())) {
        return exit_status;
    }
    const int sfx_filesize = micro_get_sfx_filesize();
    dbgprintf("myfinalsize is %d\n", sfx_filesize);
    zend_file_handle file_handle;
    const char *self_filename_mb = micro_get_filename();
    php_self = (char *)micro_get_filename();
    script_filename = (char *)micro_get_filename();
    dbgprintf("self is %s\n", self_filename_mb);

    /*
     * Do not move this initialization. It needs to happen before argv is used
     * in any way.
     */
    argv = save_ps_args(argc, argv);

    char *translated_path;
    // prepare our ini entries with stub
    char *ini_entries = malloc(sizeof(HARDCODED_INI) + micro_ext_ini.size);
    memcpy(ini_entries, HARDCODED_INI, sizeof(HARDCODED_INI));
    size_t ini_entries_len = sizeof(HARDCODED_INI);
    if (0 < micro_ext_ini.size) {
        memcpy(&ini_entries[ini_entries_len - 2], micro_ext_ini.data, micro_ext_ini.size);
        ini_entries_len += micro_ext_ini.size - 2;
        free(micro_ext_ini.data);
        micro_ext_ini.data = NULL;
    }
    // remove ending 2 '\0's
    ini_entries_len -= 2;

    sapi_module_struct *sapi_module = &micro_sapi_module;
    int module_started = 0, request_started = 0, sapi_started = 0;

#if defined(PHP_WIN32) && !defined(PHP_MICRO_WIN32_NO_CONSOLE)
    php_win32_console_fileno_set_vt100(STDOUT_FILENO, TRUE);
    php_win32_console_fileno_set_vt100(STDERR_FILENO, TRUE);
#endif

    micro_sapi_module.additional_functions = additional_functions;

#if false && defined(PHP_WIN32) && defined(_DEBUG) && defined(PHP_WIN32_DEBUG_HEAP)
    {
        int tmp_flag;
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        tmp_flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        tmp_flag |= _CRTDBG_DELAY_FREE_MEM_DF;
        tmp_flag |= _CRTDBG_LEAK_CHECK_DF;

        _CrtSetDbgFlag(tmp_flag);
    }
#endif

#if defined(SIGPIPE) && defined(SIG_IGN)
    signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE in standalone mode so
                                that sockets created via fsockopen()
                                don't kill PHP if the remote site
                                closes it.  in apache|apxs mode apache
                                does that for us!  thies@thieso.net
                                20000419 */
#endif

#ifdef ZTS
    php_tsrm_startup();
#    ifdef PHP_WIN32
    ZEND_TSRMLS_CACHE_UPDATE();
#    endif
#endif

    zend_signal_startup();

#ifdef PHP_WIN32
    int wapiret = 0;
    php_win32_init_random_bytes();
    // php_win32_signal_ctrl_handler_init();
    php_win32_ioutil_init();
    php_win32_init_gettimeofday();

    _fmode = _O_BINARY;                 /* sets default for file streams to binary */
    setmode(_fileno(stdin), O_BINARY);  /* make the stdio mode be binary */
    setmode(_fileno(stdout), O_BINARY); /* make the stdio mode be binary */
    setmode(_fileno(stderr), O_BINARY); /* make the stdio mode be binary */
#endif

    // here we start execution
    dbgprintf("start try catch\n");
    zend_first_try {
        sapi_module->ini_defaults = sapi_micro_ini_defaults;
        // this should not be settable
        // TODO: macro to configure or special things
        // sapi_module->php_ini_path_override = ini_path_override;
        sapi_module->phpinfo_as_text = 1;
        sapi_module->php_ini_ignore_cwd = 1;
        sapi_module->php_ini_ignore = 1;
        dbgprintf("start sapi\n");
        sapi_startup(sapi_module);
        sapi_started = 1;

        sapi_module->executable_location = argv[0];
        sapi_module->ini_entries = ini_entries;

        /* startup after we get the above ini override se we get things right */
        dbgprintf("start minit\n");
        if (sapi_module->startup(sapi_module) == FAILURE) {
            dbgprintf("failed minit\n");
            exit_status = 1;
            goto out;
        }

        // omit PHP_BINARY constant
        // or let PHP_BINARY constant <- micro.php_binary if setted in ini
        if (SUCCESS == zend_hash_str_del(EG(zend_constants), "PHP_BINARY", sizeof("PHP_BINARY") - 1)) {
            dbgprintf("remake pb constant\n");
            zend_ini_entry *pbentry = NULL;
            if (NULL !=
                (pbentry = zend_hash_str_find_ptr(
                     EG(ini_directives), PHP_MICRO_INIENTRY(php_binary), sizeof(PHP_MICRO_INIENTRY(php_binary)) - 1))) {
                dbgprintf("to %s\n", ZSTR_VAL(pbentry->value));
                REGISTER_MAIN_STRINGL_CONSTANT("PHP_BINARY",
                    ZSTR_VAL(pbentry->value),
                    ZSTR_LEN(pbentry->value),
                    CONST_PERSISTENT | CONST_CS | CONST_NO_FILE_CACHE);
            } else {
                dbgprintf("removed\n");
                REGISTER_MAIN_STRINGL_CONSTANT("PHP_BINARY", "", 0, CONST_PERSISTENT | CONST_CS | CONST_NO_FILE_CACHE);
            }
        }
        /*
        if (SUCCESS != (exit_status = micro_open_self_stream())){
            goto out;
        }
        */

        module_started = 1;

        // hook at here for some extensions that will modify standard ops
        if (SUCCESS != (exit_status = micro_hook_plain_files_wops())) {
            // if hook failed, go error
            goto out;
        }

        // hook at here for some extensions that will register proto
        if (SUCCESS != (exit_status = micro_reregister_proto("phar"))) {
            // if hook failed, go error
            goto out;
        }

        FILE *fp = VCWD_FOPEN(self_filename_mb, "rb");
        zend_fseek(fp, sfx_filesize, SEEK_SET);

        dbgprintf("fin opening self\n");
        if (!fp) {
            dbgprintf("Could not open self.\n");
            goto err;
        }

        // no chdir as cli do
        SG(options) |= SAPI_OPTION_NO_CHDIR;

        zend_stream_init_fp(&file_handle, fp, self_filename_mb);

        dbgprintf("set args\n");
        SG(request_info).argc = argc;
        char real_path[MAXPATHLEN];
        if (VCWD_REALPATH(self_filename_mb, real_path)) {
            translated_path = strdup(real_path);
        }
        SG(request_info).path_translated = translated_path; // tofree
        SG(request_info).argv = argv;

        dbgprintf("start rinit\n");
        if (php_request_startup() == FAILURE) {
            fclose(file_handle.handle.fp);
            dbgprintf("failed rinit\n");
            goto err;
        }
        dbgprintf("done rinit\n");
        request_started = 1;

        // add STD{OUT, IN, ERR} constants
        micro_register_file_handles();
        CG(skip_shebang) = 1;
        zend_register_bool_constant(
            ZEND_STRL("PHP_CLI_PROCESS_TITLE"), is_ps_title_available() == PS_TITLE_SUCCESS, CONST_CS, 0);

        // ?
        zend_is_auto_global_str(ZEND_STRL("_SERVER"));

        PG(during_request_startup) = 0;

        dbgprintf("start execution\n");
        php_execute_script(&file_handle);
        exit_status = EG(exit_status);
    }
    zend_end_try();

out:
    // frees here
    if (request_started) {
        dbgprintf("rshutdown\n");
        php_request_shutdown((void *)0);
    }
    if (translated_path) {
        free(translated_path);
    }
    if (exit_status == 0) {
        exit_status = EG(exit_status);
    }
    if (ini_entries) {
        free(ini_entries);
    }
    micro_free_reregistered_protos();
    if (module_started) {
        dbgprintf("mshutdown\n");
        php_module_shutdown();
    }
    if (sapi_started) {
        dbgprintf("sapishutdown\n");
        sapi_shutdown();
    }
#ifdef ZTS
    dbgprintf("tsrmshutdown\n");
    tsrm_shutdown();
#endif
    cleanup_ps_args(argv);
    return exit_status;
err:
    sapi_deactivate();
    zend_ini_deactivate();
    exit_status = 1;
    goto out;

    // never reach here
    return 0;
}
/* }}} */
