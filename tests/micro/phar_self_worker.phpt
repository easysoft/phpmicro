--TEST--
phar:// access to the running sfx from a Swow worker thread
--SKIPIF--
<?php
if ("micro" !== php_sapi_name()) {
    die("skip micro SAPI only");
}
if (!extension_loaded("phar")) {
    die("skip phar extension required");
}
if (!extension_loaded("swow")) {
    die("skip swow extension required");
}
if (!PHP_ZTS) {
    die("skip Swow\\Thread requires ZTS");
}
if (!class_exists(\Swow\Thread::class)) {
    die("skip Swow\\Thread required");
}
?>
--INI--
phar.readonly=0
register_argc_argv=0
--FILE--
<?php

chdir(__DIR__);

$pharFile = 'phar_self_worker.phar';
$exeFile = 'phar_self_worker';

@unlink($pharFile);
@unlink($exeFile);

$stub = <<<'STUB'
<?php

$self = str_replace('\\', '/', micro_get_self_filename());
$foo = 'phar://' . $self . '/foo.php';

echo "main is_file=", var_export(is_file($foo), true), "\n";
$result = include $foo;
echo "main include=", var_export($result, true), "\n";

$code = <<<'CODE'
$self = str_replace('\\', '/', micro_get_self_filename());
$dir = opendir('phar://' . $self . '/');
$names = [];
if (is_resource($dir)) {
    while (false !== ($name = readdir($dir))) {
        $names[] = $name;
    }
    closedir($dir);
}
sort($names);
echo "worker opendir=", implode(',', $names), "\n";
$foo = 'phar://' . $self . '/foo.php';
echo "worker is_file=", var_export(is_file($foo), true), "\n";
$result = include $foo;
echo "worker include=", var_export($result, true), "\n";
$fresh = 'phar://' . $self . '/baz.php';
echo "worker fresh_is_file=", var_export(is_file($fresh), true), "\n";
$result = include $fresh;
echo "worker fresh_include=", var_export($result, true), "\n";
$handle = fopen($fresh, 'rb');
echo "worker fopen=", var_export(is_resource($handle), true), "\n";
if (is_resource($handle)) {
    echo "worker fseek=", var_export(fseek($handle, 0, SEEK_SET), true), "\n";
    echo "worker fread=", bin2hex(fread($handle, 5)), "\n";
    fclose($handle);
}
CODE;

\Swow\Thread::runCode($code)->wait();

echo "done\n";

__HALT_COMPILER();
STUB;

$phar = new Phar($pharFile);
$phar->startBuffering();
$phar->addFromString('foo.php', "<?php\necho \"foo.php executed\\n\";\n");
$phar->addFromString('baz.php', "<?php\necho \"baz.php executed\\n\";\n");
$phar->setStub($stub);
$phar->stopBuffering();
unset($phar);

/* the sfx image includes the ini block of this test, so the copy also
 * carries phar.readonly and register_argc_argv into the generated executable */
$size = micro_get_sfxsize();
$self = micro_open_self();
$out = fopen($exeFile, 'wb');
$copied = stream_copy_to_stream($self, $out, $size);
if ($copied !== $size) {
    fwrite(STDERR, "failed to copy the sfx header\n");
    exit(1);
}
$in = fopen($pharFile, 'rb');
stream_copy_to_stream($in, $out);
fclose($in);
fclose($out);
fclose($self);
chmod($exeFile, 0755);

passthru('./' . $exeFile, $status);
var_dump($status);
?>
--CLEAN--
<?php

chdir(__DIR__);

@unlink('phar_self_worker.phar');
@unlink('phar_self_worker');
?>
--EXPECT--
main is_file=true
foo.php executed
main include=1
worker opendir=baz.php,foo.php
worker is_file=true
foo.php executed
worker include=1
worker fresh_is_file=true
baz.php executed
worker fresh_include=1
worker fopen=true
worker fseek=0
worker fread=3c3f706870
done
int(0)
