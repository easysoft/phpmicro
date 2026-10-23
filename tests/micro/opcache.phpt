--TEST--
Test micro with opcache enabled
--SKIPIF--
<?php
if ("micro" !== php_sapi_name()) {
    die("SKIP because this test is only for micro SAPI");
}
if (!extension_loaded("Zend OPcache")) {
    die("SKIP because the opcache extension is required");
}
?>
--INI--
opcache.enable=1
opcache.enable_cli=1
--FILE--
<?php
$status = opcache_get_status(false);
var_dump($status !== false && $status["opcache_enabled"]);
echo "done\n";
?>
--EXPECT--
bool(true)
done
