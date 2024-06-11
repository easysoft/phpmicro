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
