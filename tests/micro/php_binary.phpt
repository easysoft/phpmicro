--TEST--
Test if PHP_BINARY is remade correctly
--SKIPIF--
<?php
if("micro" !== php_sapi_name()){
    die("SKIP because this test is only for micro SAPI");
}
--INI--
micro.php_binary=cafebabe
--FILE--
<?php
var_dump(PHP_BINARY);
if(false !== ini_set("micro.php_binary", "deadbeef")){
    echo "micro.php_binary is settable!\n";
}
var_dump(PHP_BINARY);
?>
--EXPECT--
string(8) "cafebabe"
string(8) "cafebabe"
