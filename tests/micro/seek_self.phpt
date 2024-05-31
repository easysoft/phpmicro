--TEST--
Test fseek() fstat() with micro hooks
--SKIPIF--
<?php
if("micro" !== php_sapi_name()){
    die("SKIP because this test is only for micro SAPI");
}
--FILE--
<?php

$self = fopen(__FILE__, 'r');

var_dump(fread($self, 4));

$ret = fseek($self, -4, SEEK_SET);
var_dump($ret);
var_dump(ftell($self));
var_dump(fread($self, 4));

$ret = fseek($self, 0, SEEK_SET);
var_dump($ret);
var_dump(ftell($self));
var_dump(fread($self, 4));

$ret = fseek($self, 1, SEEK_SET);
var_dump($ret);
var_dump(ftell($self));
var_dump(fread($self, 4));

$ret = fseek($self, 0, SEEK_CUR);
var_dump($ret);
var_dump(ftell($self));
var_dump(fread($self, 4));

$ret = fseek($self, -4-$ret, SEEK_CUR);
var_dump($ret);
var_dump(ftell($self));
var_dump(fread($self, 4));

$ret = fseek($self, 999999, SEEK_CUR);
var_dump($ret);
var_dump(ftell($self));
var_dump(fread($self, 4));

$ret = fseek($self, 0, SEEK_END);
var_dump($ret);
var_dump(fread($self, 4));

$fileSize = ftell($self);

$ret = fseek($self, 4, SEEK_END);
var_dump($ret);
var_dump(fread($self, 4));

if ($fileSize + 4 != ftell($self)) {
    var_dump(ftell($self), $fileSize);
    echo "Failed to seek to the end of the file\n";
}

$ret = fseek($self, -99999, SEEK_END);
var_dump($ret);
var_dump(fread($self, 4));

if ($fileSize + 4 != ftell($self)) {
    var_dump(ftell($self), $fileSize);
    echo "Failed to seek to the end of the file\n";
}

$ret = fseek($self, 0, SEEK_CUR);
var_dump($ret);
var_dump(fread($self, 4));

if ($fileSize + 4 != ftell($self)) {
    var_dump(ftell($self), $fileSize);
    echo "Failed to seek to the end of the file\n";
}

if (fstat($self)['size'] != $fileSize) {
    var_dump(fstat($self)['size'], $fileSize);
    echo "Failed to seek to the end of the file\n";
}

echo "Done\n";
?>
--EXPECTF--
string(4) "<?ph"
int(-1)
int(4)
string(0) ""
int(0)
int(0)
string(4) "<?ph"
int(0)
int(1)
string(4) "?php"
int(0)
int(5)
string(4) "

$s"
int(0)
int(5)
string(4) "

$s"
int(0)
int(1000008)
string(0) ""
int(0)
string(0) ""
int(0)
string(0) ""
int(-1)
string(0) ""
int(0)
string(0) ""
Done
