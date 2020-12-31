<?php
// fake a cli command, for run tests.php
$mainpath = NULL;
$orig_argv = $argv;
$arg0 = array_shift($argv);
$inisets = "micro.php_binary=$arg0\n";
$errf = STDERR;
//$errf = fopen("/tmp/fakecmd.log", "ab+");
function argvalue2($arg){
    global $argv;
    global $errf;
    if(strlen($arg) === 2){
        $v = array_shift($argv);
    }else{
        $v = substr($arg, 2);
    }
    if(NULL === $v){
        fprintf($errf, "failed parse arg\n");
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
            fprintf($errf, "not support for stdin codes yet\n");
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
        case "-C":
        case "-q":
        case "-n":
            // do nothing due to micro donot
            //  - chdir
            //  - output HTTP header
            //  - consume php.ini.
            goto gonext;
        case "-d":
            $def = argvalue2($arg);
            $kv = explode("=", $def, 2);
            if(count($kv) > 1){
                if(
                    strlen($kv[1]) > 0 && // *val != '\0'
                    false === strpos("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890", $kv[1][0]) // !isalnum(*val) && *val != '"' && *val != '\''
                ){
                    $inisets .= $kv[0] . '="' . $kv[1] . "\"\n";
                }else{
                    $inisets .= $kv[0] . '=' . $kv[1] . "\n";
                }
            }else{
                $inisets .= $def . "=1\n";
            }
            goto gonext;
        case "-e":
            fprintf($errf, "not support for stdin codes yet\n");
            exit(1);
        case "-f":
            if($modeset){
                fprintf($errf, "mode differ\n");
                exit(1);
            }
            $modeset = true;
            $mainpath= argvalue2($arg);
            goto gonext;
        case "-?":
        case "-h":
            fprintf(STDOUT, "no help yet\n");
            exit(0);
        case "-i":
            phpinfo();
            exit(0);
        case "-l":
            fprintf($errf, "no lint yet\n");
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
                fprintf($errf, "mode differ\n");
                exit(1);
            }
            $modeset = true;
            $maincode = argvalue2($arg);
            goto gonext;
        case "-B":
        case "-R":
        case "-F":
        case "-E":
            fprintf($errf, "-{B, R, F, E} not implement yet\n");
            exit(1);
        case "-H":
            fprintf($errf, "-H not implement yet\n");
            exit(1);
        case "-S":
        case "-t":
            fprintf($errf, "-{S, t} not implement yet\n");
            exit(1);
        case "-s":
            fprintf($errf, "-s not implement yet\n");
            exit(1);
        case "-v":
            echo "PHP " . PHP_VERSION . " (micro) (built: Jan 01 1970 00:00:00) ( NTS )\n" .
                "Copyright (c) The PHP Group\n" .
                "Zend Engine v0.0.0, Copyright (c) Zend Technologies\n" .
                extension_loaded("Zend Opcache")? "with Zend OPcache v" . PHP_VERSION . ", Copyright (c), by Zend Technologies" : "" .
                "\n";
            exit(0);
        case "-w":
            fprintf($errf, "-w not implement yet\n");
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
                    fprintf($errf, "--r{f, c, e, z, i} not implement yet\n");
                    exit(1);
                case "--":
                    $arg = array_shift($argv);
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
    fprintf($errf, "run waht?\n");
    exit(1);
}


set_error_handler(function(?int $errno = 0, ?string $errstr = "", ?string $errfile = "", ?int $errline = 0, ?array $errcontext = NULL) use ($orig_argv){
    global $errf;
    $fullcmd = implode(" ", $orig_argv);
    fprintf($errf, "$errstr \n at $errfile:$errline\n while processing cmd:\n $fullcmd\n");
    exit(1);
});

const BUFSIZE = 4096;
function writesfx($out){
    $sfx = fopen(micro_get_self_filename(), "rb");
    // write sfx header
    $size = micro_get_sfx_filesize();
    for(; $size > 0; $size -= BUFSIZE){
        fwrite($out, fread($sfx, $size > BUFSIZE ? BUFSIZE : $size));
    }
    fclose($sfx);
}

function writeinis($out, $inisets){
    if(strlen($inisets)>0){
        fwrite($out, "\xfd\xf6\x69\xe6");
        fwrite($out, pack("N", strlen($inisets)));
        fwrite($out, $inisets);
    }
}

function writecode($out, $path){
    $in = fopen($path, "rb");
    do{
        $wrote = fwrite($out, fread($in, BUFSIZE));
    }while(BUFSIZE === $wrote);
    fclose($in);
}

$ret = NULL;
if(isset($mainpath)){
    $outpath = $mainpath . ".exe";
    $out = fopen($outpath, "wb");

    writesfx($out);
    writeinis($out, $inisets);
    writecode($out, $mainpath);
    fclose($out);
    chmod($outpath, 0755);

    // switch php and exe
    rename($mainpath, $mainpath . ".orig");
    rename($outpath, $mainpath);
    //copy($mainpath, $outpath);
    passthru($mainpath . " " . implode(" ", $argv), $ret);
    rename($mainpath . ".orig", $mainpath);
}else{
    $outpath = "./temp.exe";
    $out = fopen($outpath, "wb");

    writesfx($out);
    writeinis($out, $inisets);
    fwrite($out, "\x3c?php " .$maincode);
    fclose($out);
    chmod($outpath, 0755);

    passthru($outpath . " " . implode(" ", $argv), $ret);
}


// remove temp executable
if(is_file($outpath)){
    unlink($outpath);
}
exit($ret);
