--TEST--
Test $argv variable things
--SKIPIF--
<?php
if("micro" !== php_sapi_name()){
    die("SKIP because this test is only for micro SAPI");
}
--ARGS--
-param1 -p2 /p3 123 321
--FILE--
<?php
var_dump($_SERVER["argv"], $_SERVER["argc"]);
var_dump($argv, $argc);
--EXPECTF--
array(6) {
  [0]=>
  string(%d) "%sargvs.php"
  [1]=>
  string(7) "-param1"
  [2]=>
  string(3) "-p2"
  [3]=>
  string(3) "/p3"
  [4]=>
  string(3) "123"
  [5]=>
  string(3) "321"
}
int(6)
array(6) {
  [0]=>
  string(%d) "%sargvs.php"
  [1]=>
  string(7) "-param1"
  [2]=>
  string(3) "-p2"
  [3]=>
  string(3) "/p3"
  [4]=>
  string(3) "123"
  [5]=>
  string(3) "321"
}
int(6)
