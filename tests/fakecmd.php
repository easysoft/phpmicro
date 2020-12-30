<?php
// fake a cli command
$stderr = fopen("php://stderr", "r+b");
$mainprog = NULL;
$inisets = "";
$arg0 = array_shift($argv);
for($arg = array_shift($argv); NULL !== $argv; $arg = array_shift($argv)){
    if("-n" === $arg){
        // do nothing due to micro donot consume php.ini.
        continue;
    }
    if(0 === strpos($arg, "-d")){
        if(2 === strlen($arg)){
            $def = array_shift($argv);
            if(NULL === $def){
                fprintf($stderr, "define waht?\n");
                exit(1);
            }
        }else{
            $def = substr($arg, 2);
        }
        $kv = explode("=", $def, 2);
        if(count($kv) > 1){
            $inisets .= $kv[0] . '="' . $kv[1] . "\"\n";
        }else{
            $inisets .= $def . "\n";
        }
        continue;
    }
    if("-f" === $arg){
        $mainprog = array_shift($argv);
        continue;
    }
    if("--" === $arg){
        break;
    }
    if(!$mainprog){
        $mainprog = $arg;
    }else {
        array_unshift($argv, $arg);
    }
    break;
}
if(!$mainprog){
    fprintf($stderr, "run waht?\n");
    exit(1);
}

$outpath = $mainprog . ".exe";
$out = fopen($outpath, "wb");
$sfx = fopen(micro_get_self_filename(), "rb");
$size = micro_get_sfx_filesize();
$bufsize = 4096;
for(; $size > 0; $size -= $bufsize){
    fwrite($out, fread($sfx, $size > $bufsize ? $bufsize : $size));
}
fclose($sfx);
if(strlen($inisets)>0){
    fwrite($out, "\xfd\xf6\x69\xe6");
    fwrite($out, pack("N", strlen($inisets)));
    fwrite($out, $inisets);
}
$mainfile = fopen($mainprog, "rb");
do{
    $wrote = fwrite($out, fread($mainfile, $bufsize));
}while ($wrote === $bufsize);
fclose($out);
chmod($outpath, 0755);

$ret = NULL;
passthru($outpath . " " . implode(" ", $argv), $ret);
// remove temp executable
unlink($outpath);
exit($ret);
