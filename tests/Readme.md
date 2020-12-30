# Tests

这些测试或许可以通过修改的run-tests.php的命令行选项跳过

these tests may be skipped in command line via modiffied run-tests.php

 - tests/basic/bug54514.phpt: micro donot run like this
 - tests/basic/bug71273.phpt: TODO: write micro flavor test like this
 - tests/basic/consistent_float_string_casts.phpt: setlocale(3) may not be usable in static build
 - tests/lang/bug30638.phpt: setlocale(3) may not be usable in static build
