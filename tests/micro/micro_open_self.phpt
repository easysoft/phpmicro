--TEST--
Test micro_open_self() function
--SKIPIF--
<?php
if("micro" !== php_sapi_name()){
    die("SKIP because this test is only for micro SAPI");
}
--FILE--
<?php
$full = micro_open_self();
switch(PHP_OS) {
    case 'Unix':
    case 'FreeBSD':
    case 'NetBSD':
    case 'OpenBSD':
    case 'Linux':
    case 'IRIX64':
    case 'SunOS':
    case 'HP-UX':
        $header = fread($full, 4);
        $ok = $header === "\x7fELF";
        break;
    case 'WINNT':
    case 'WIN32':
    case 'Windows':
    case 'CYGWIN_NT':
        $header = fread($full, 2);
        $ok = $header === "MZ";
        break;
    case 'Darwin':
        $header = fread($full, 4);
        $ok =
            $header === "\xcf\xfa\xed\xfe" || // le32 0xfeedfacf: 64bit macho
            $header === "\xce\xfa\xed\xfe" || // le32 0xfeedface: 32bit macho
            $header === "\xbe\xba\xfe\xca" // le32 0xcafebabe: universal
        ;
        break;
}

if(!$ok){
    echo "bad executable header\n";
}

fclose($full);

--EXPECTF--
