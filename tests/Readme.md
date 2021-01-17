# Tests

这些测试或许可以通过修改的run-tests.php的命令行选项跳过

these tests may be skipped in command line via modiffied run-tests.php

 - tests/basic/bug71273.phpt: TODO: write micro flavor test like this
 - tests/basic/consistent_float_string_casts.phpt: setlocale(3) may not be usable in static build
 - tests/lang/bug30638.phpt: setlocale(3) may not be usable in static build
 - Zend/tests/bug40236.phpt: fakephp has not -a support
 - Zend/tests/lc_ctype_inheritance.phpt: setlocale(3) may not be usable in static build
 - ext/phar/tests/cache_list/copyonwrite*.phar.php micro donot support write on self file
 - ext/pcntl/tests/pcntl_exec.phpt: fakephp not supported stdin codes yet
 - ext/standard/tests/general_functions/phpinfo.phpt: micro's phpinfo() is no same as cli
 - ext/standard/tests/url/get_headers_error_003.phpt: micro should not use cli "-S" command line option
 - ext/standard/tests/versioning/php_sapi_name.phpt: micro do not have a SAPI name "c$ci"

these tests may failed if not using cli_checks.patch because of php internel sapi name checks.

 - tests/lang/bug45392.phpt
