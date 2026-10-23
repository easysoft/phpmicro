--TEST--
Test stdio constants and php://fd with micro hooks
--SKIPIF--
<?php
if ("micro" !== php_sapi_name()) {
    die("SKIP because this test is only for micro SAPI");
}
?>
--FILE--
<?php
var_dump(defined("STDIN"), defined("STDOUT"), defined("STDERR"));

$fd = fopen("php://fd/1", "wb");
var_dump($fd !== false);
if ($fd) {
    fwrite($fd, "php://fd\n");
    fclose($fd);
}

fwrite(STDOUT, "STDOUT\n");
fflush(STDOUT);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
php://fd
STDOUT
