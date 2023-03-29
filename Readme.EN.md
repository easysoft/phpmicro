# micro self-executable SAPI for PHP

[Chinese version](Readme.md)

![php](https://img.shields.io/badge/php-8.0--8.2-royalblue.svg)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![tests](https://github.com/dixyes/phpmicro/actions/workflows/tests.yml/badge.svg)](https://github.com/dixyes/phpmicro/actions/workflows/tests.yml)

micro self-executable SAPI makes PHP self-executable.

Just concatenate micro.sfx and random php source file or phar into a single file to use it.

## Compatibility

Yet only support PHP8+; Windows, Linux, macOS.

## Fetch and Usage

There's a micro micro.sfx binary contains minimal extensions set builds automatically in Github actions. If you need extensions, build your own micro or use [crazywhalecc/static-php-cli](https://github.com/crazywhalecc/static-php-cli) <!-- [automatic build system (not completed yet)](https://github.com/dixyes/lwmbs/actions) -->

Just concatenate micro.sfx and php source to use it.

For example: if the content of myawesomeapp.php is

```php
<?php
echo "hello, this is my awesome app." . PHP_EOL;
```

at Linux/macOS:

Note: If you downloaded micro.sfx and macOS does not let you execute it, try:

```bash
sudo xattr -d com.apple.quarantine /path/to/micro.sfx
```

then

```bash
cat /path/to/micro.sfx myawesomeapp.php > myawesomeapp
chmod 0755 ./myawesomeapp
./myawesomeapp
# show "hello, this is my awesome app."
```

or Windows:

```batch
COPY /b \path\to\micro.sfx + myawesomeapp.php myawesomeapp.exe
myawesomeapp.exe
REM show "hello, this is my awesome app."
```

## Build micro.sfx

### Preparation

1.Clone this repo into sapi/micro under PHP source

```bash
# at PHP source dir
git clone <url for this repo> sapi/micro
```

2.Apply patches

Patches are located at patches directory, choose patch(es) as you like, see [Readme.md](patches/Readme.md) in patches dir for detail

Apply patch:

```bash
# at PHP source dir
patch -p1 < sapi/micro/patches/<name of patch>
```

### UNIX-like Build

0.Prepare build environment according to official PHP documents.

1.buildconf

```bash
# at PHP source dir
./buildconf --force
```

2.configure

```bash
# at PHP source dir
./configure <options>
```

Options for reference:

`--disable-phpdbg --disable-cgi --disable-cli --disable-all --enable-micro --enable-phar --with-ffi --enable-zlib`

At Linux libc compatibility can be a problem, micro provides two kinds of `configure` argument:

- `--enable-micro=yes`or`--enable-micro`: this will make PIE shared ELF micro sfx, this kind of binary cannot be invoked cross libc (i.e. you cannot run such a binary which built on alpine with musl on any glibc-based CentOS), but the binary can do ffi and PHP `dl()` function.
- `--enable-micro=all-static`: this will make static ELF micro sfx, this kind of binary can even run barely on the top of the kernel, but ffi/`dl()` is not supported.

3.make

```bash
# at PHP source dir
make micro
```

(`make all`(aka. `make`) may work also, but only build micro SAPI -s recommended.

That built file is located at sapi/micro/micro.sfx.

### Windows Build

0.Prepare build environment according to official PHP documents.

1.buildconf

```batch
# at PHP source dir
buildconf
```

2.configure

```batch
# at PHP source dir
configure <options>
```

Options for reference:

`--disable-all --disable-zts --enable-micro --enable-phar --with-ffi --enable-zlib`

3.make
Due to PHP build system on Windows lack of ability to statically build PHP binary, you cannot build micro with `nmake`

```batch
# at PHP source dir
nmake micro
```

That built file is located at `<arch name like x64>\\<configuration like Release>\\micro.sfx`.

## Optimizations

Hugepages optimization for Linux in PHP build system insults huge size of sfx, if you do not take advantage of hugepages, use disable_huge_page.patch to shrink sfx size.

Statically build under Linux needs libc, most common glibc may be large, musl is recommended hence. manually installed musl or some distros provided musl will provide `musl-gcc` or `musl-clang` wrapper, use one of them before configure by specify CC/CXX environ, for example

```bash
# ./buildconf things...
export CC=musl-gcc
export CXX=musl-gcc
# ./configure balabala
# make balabala
```

We hope all dependencies are statically linked into sfx, however, some distro do not provide static version of them, we may manually build them, the case of libffi (ffi extension is not supported in `all-static` builds):

```bash
# fetch sources througe git
git clone https://github.com/libffi/libffi
cd libffi
git checkout <version you like, v3.3 for example>
autoreconf -i
# or download tarball
wget <url>
tar xf <tar name>
cd <extracted name>
# if we use musl
export CC=musl-gcc
export CXX=musl-gcc
# build, install it
./configure --prefix=/my/prefered/path &&
make -j`nproc` &&
make install
```

then build micro as

```bash
# ./buildconf things...
# export CC=musl-xxx things...
export PKG_CONFIG_PATH=/my/prefered/path/lib/pkgconfig
# ./configure balabala
# make balabala
```

## Some details

### ini settings

See wiki：[INI-settings](https://github.com/easysoft/phpmicro/wiki/INI-settings)(TODO: en version)

### PHP_BINARY constant

In micro, `PHP_BINARY` is an empty string, you can use an ini setting to modify it: `micro.php_binary=somestring`

## OSS License

```plain
Copyright 2020 Longyan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## remind me to update the English readme and fix typos and strange or offensive expressions

