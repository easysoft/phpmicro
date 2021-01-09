--TEST--
Test micro_version() function
--SKIPIF--
<?php
if("micro" !== php_sapi_name()){
    die("SKIP because this test is only for micro SAPI");
}
--FILE--
<?php
var_dump(micro_version()[0], micro_version()[1], micro_version()[2], isset(micro_version()[3])?micro_version()[3]:"none");
--EXPECTF--
int(%d)
int(%d)
int(%d)
string(%d) "%s"
