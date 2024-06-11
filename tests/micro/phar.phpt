--TEST--
Test $argv variable things
--SKIPIF--
<?php
if("micro" !== php_sapi_name()){
    die("SKIP because this test is only for micro SAPI");
}
if(!extension_loaded("phar")){
    die("SKIP because phar extension is required");
}
?>
--INI--
phar.readonly=0
--FILE--
<?php

chdir(__DIR__);

@unlink("testphar.phar");
@unlink("testphar.phar.gz");

$phar = new Phar("testphar.phar");

$index = <<<'PHP'
<?php

echo "index.php\n";

echo "include seek_self.inc from phar\n";
include_once __DIR__ . "/seek_self.inc";

echo "include seek_self.inc from dir\n";
include_once getcwd() . "/seek_self.inc";

echo "include lv1.php from phar\n";
include_once __DIR__ . "/lv1.php";

echo "list phar dir\n";
$dir = opendir(__DIR__);
$items = [];
while($item = readdir($dir)){
    $items[] = $item;
}
sort($items);
var_dump($items);

PHP;

$lv1 = <<<'PHP'
<?php

echo "lv1.php\n";

echo "include lv2.php from phar\n";
include_once __DIR__ . "/lv2.php";

PHP;

$lv2 = <<<'PHP'
<?php

echo "lv2.php\n";

echo "include lv3.php from phar\n";
include_once __DIR__ . "/lv3.php";

PHP;

$lv3 = <<<'PHP'
<?php

echo "lv3.php\n";

PHP;

$phar->startBuffering();
$phar->addFromString("index.php", $index);
$phar->addFromString("lv1.php", $lv1);
$phar->addFromString("lv2.php", $lv2);
$phar->addFromString("lv3.php", $lv3);
// NOTE: seek_self.inc have different behavior in phar
// so the expect is different, this is not a bug.
$phar->addFile("seek_self.inc", "seek_self.inc");
$phar->stopBuffering();

if ($phar->canCompress(Phar::GZ)) {
    $phar->compress(Phar::GZ);
}
$phar->setDefaultStub("index.php", "index.php");

$selffile = micro_open_self();
$exe = fopen("phartest.exe", "wb");
for ($i = 0; $i < micro_get_sfxsize(); $i += 8192) {
    $read = fread($selffile, micro_get_sfxsize() - $i > 8192 ? 8192 : micro_get_sfxsize() - $i);
    fwrite($exe, $read);
}

fwrite($exe, file_get_contents("testphar.phar"));
fflush($exe);
fclose($exe);
fclose($selffile);

chmod("phartest.exe", 0755);

passthru("./phartest.exe", $ret);
if ($ret !== 0) {
    echo "Failed to execute phar\n";
    exit(1);
}

echo "Done\n";
?>
--CLEAN--
<?php

chdir(__DIR__);

@unlink("testphar.phar");
@unlink("testphar.phar.gz");
@unlink("phartest.exe");

?>
--EXPECTF--
index.php
include seek_self.inc from phar
string(4) "<?ph"
int(-1)
bool(false)
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
int(-1)
bool(false)
string(0) ""
int(0)
string(0) ""
int(-1)
string(0) ""
bool(false)
int(1585)
Failed to seek to the end of the file
int(-1)
string(0) ""
bool(false)
int(1585)
Failed to seek to the end of the file
int(-1)
string(0) ""
bool(false)
int(1585)
Failed to seek to the end of the file
Done
include seek_self.inc from dir
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
include lv1.php from phar
lv1.php
include lv2.php from phar
lv2.php
include lv3.php from phar
lv3.php
list phar dir
array(5) {
  [0]=>
  string(9) "index.php"
  [1]=>
  string(7) "lv1.php"
  [2]=>
  string(7) "lv2.php"
  [3]=>
  string(7) "lv3.php"
  [4]=>
  string(13) "seek_self.inc"
}
Done
