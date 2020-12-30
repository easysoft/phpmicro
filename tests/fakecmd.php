<?php
// fake a cli command
$mainpath = NULL;
$inisets = "";
$orig_argv = $argv;
$arg0 = array_shift($argv);
function argvalue2($arg){
    global $argv;
    if(strlen($arg) === 2){
        $v = array_shift($argv);
    }else{
        $v = substr($arg, 2);
    }
    if(NULL === $v){
        fprintf(STDERR, "failed parse arg\n");
        exit(1);
    }
    return $v;
}
function pathjoin($a, $b){
    return rtrim($a, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $b;
}
function addinifile($f){
    global $inisets;
    if(false === ($ini = @file_get_contents($f))){
        return;
    }
    $inisets .= $ini . "\n";
    unset($ini);
}
$modeset = false;
for($arg = array_shift($argv); NULL !== $argv; $arg = array_shift($argv)){
    switch(substr($arg, 0, 2)){
        case "-a":
            fprintf(STDERR, "not support for stdin codes yet\n");
            exit(1);
        case "-c":
            $v = argvalue2($arg);
            if(isdir($v)){
                foreach (scandir($v) as $fn){
                    if(strpos($fn, ".ini") == strlen($fn) - 4){
                        addinifile(pathjoin($v, $fn));
                    }
                }
            }else{
                addinifile($v);
            }
            goto gonext;
        case "-n":
            // do nothing due to micro donot consume php.ini.
            goto gonext;
        case "-d":
            $def = argvalue2($argv);
            $kv = explode("=", $def, 2);
            if(count($kv) > 1){
                $inisets .= $kv[0] . '="' . $kv[1] . "\"\n";
            }else{
                $inisets .= $def . "\n";
            }
            goto gonext;
        case "-e":
            fprintf(STDERR, "not support for stdin codes yet\n");
            exit(1);
        case "-f":
            if($modeset){
                fprintf(STDERR, "mode differ\n");
                exit(1);
            }
            $modeset = true;
            $mainpath= argvalue2($argv);
            goto gonext;
        case "-h":
            fprintf(STDOUT, "no help yet\n");
            exit(0);
        case "-i":
            phpinfo();
            exit(0);
        case "-l":
            fprintf(STDERR, "no lint yet\n");
            exit(1);
        case "-m":
            $all = get_loaded_extensions();
            $zmod = get_loaded_extensions(true);
            $pmod = array_diff($all, $zmod);
            echo "[PHP Modules]\n";
            foreach($pmod as $e){
                echo "$e\n";
            }
            echo "\n[Zend Modules]\n";
            foreach($zmod as $e){
                echo "$e\n";
            }
            echo "\n";
            exit(0);
        case "-r":
            if($modeset){
                fprintf(STDERR, "mode differ\n");
                exit(1);
            }
            $modeset = true;
            $maincode = argvalue2($arg);
            goto gonext;
        case "-B":
        case "-R":
        case "-F":
        case "-E":
            fprintf(STDERR, "-{B, R, F, E} not implement yet\n");
            exit(1);
        case "-H":
            fprintf(STDERR, "-H not implement yet\n");
            exit(1);
        case "-S":
        case "-t":
            fprintf(STDERR, "-{S, t} not implement yet\n");
            exit(1);
        case "-s":
            fprintf(STDERR, "-s not implement yet\n");
            exit(1);
        case "-v":
            echo "PHP " . PHP_VERSION . " (micro) (built: Jan 01 1970 00:00:00) ( NTS )\n" .
                "Copyright (c) The PHP Group\n" .
                "Zend Engine v0.0.0, Copyright (c) Zend Technologies\n" .
                extension_loaded("Zend Opcache")? "with Zend OPcache v" . PHP_VERSION . ", Copyright (c), by Zend Technologies" : "" .
                "\n";
            exit(0);
        case "-w":
            fprintf(STDERR, "-w not implement yet\n");
            exit(1);
        case "-z":
            $v = argvalue2($arg);
            $inisets .= "zend_extension=\"$v\"\n";
            goto gonext;
        case "--":
            switch($arg){
                case "--ini":
                    echo "Configuration File (php.ini) Path: (none)\n" .
                        "Loaded Configuration File:         (none)\n" .
                        "Scan for additional .ini files in: (none)\n" .
                        "Additional .ini files parsed:      (none)\n";
                case "--rf":
                case "--rc":
                case "--re":
                case "--rz":
                case "--ri":
                    fprintf(STDERR, "--r{f, c, e, z, i} not implement yet\n");
                    exit(1);
                case "--":
                default:
                    // continue
            }
        default:
            // continue
    }

    if(!$modeset){
        $modeset = true;
        $mainpath = $arg;
    }else {
        array_unshift($argv, $arg);
    }
    break;
    gonext:
}
if(!$modeset){
    fprintf(STDERR, "run waht?\n");
    exit(1);
}

set_error_handler(function(?int $errno = 0, ?string $errstr = "", ?string $errfile = "", ?int $errline = 0, ?array $errcontext = NULL) use ($orig_argv){
    $fullcmd = implode(" ", $orig_argv);
    fprintf(STDERR, "$errstr \n at $errfile:$errline\n while processing cmd:\n $fullcmd\n");
    exit(1);
});

if(isset($mainpath)){
    $outpath = $mainpath . ".exe";
    $code = file_get_contents($mainpath);
}else{
    $outpath = "./temp.exe";
    $code = $maincode;
}

$out = fopen($outpath, "wb");
$sfx = fopen(micro_get_self_filename(), "rb");

// write sfx header
$size = micro_get_sfx_filesize();
$bufsize = 4096;
for(; $size > 0; $size -= $bufsize){
    fwrite($out, fread($sfx, $size > $bufsize ? $bufsize : $size));
}
fclose($sfx);

// write ini if provided
if(strlen($inisets)>0){
    fwrite($out, "\xfd\xf6\x69\xe6");
    fwrite($out, pack("N", strlen($inisets)));
    fwrite($out, $inisets);
}

// write code
$wrote = fwrite($out, $code);
fclose($out);

chmod($outpath, 0755);

$ret = NULL;
passthru($outpath . " " . implode(" ", $argv), $ret);
// remove temp executable
unlink($outpath);
exit($ret);
