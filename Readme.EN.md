# micro self-executable SAPI for PHP

[![Chinese readme](https://img.shields.io/badge/README-%E4%B8%AD%E6%96%87%20%F0%9F%87%A8%F0%9F%87%B3-white)](Readme.md)
![php](https://img.shields.io/badge/php-8.0--8.2-royalblue.svg)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![tests](https://github.com/dixyes/phpmicro/actions/workflows/tests.yml/badge.svg)](https://github.com/dixyes/phpmicro/actions/workflows/tests.yml)

micro self-executable SAPI makes PHP self-executable.

Just concatenate micro.sfx and random php source file or phar into a single file to use it.

## Compatibility

Yet only support PHP8+; Windows, Linux, macOS.

## Fetch and Usage

A micro `micro.sfx` binary containing the minimal extensions set is built automatically in [Github Actions](actions). If you need more extensions, build your own micro or use [crazywhalecc/static-php-cli](https://github.com/crazywhalecc/static-php-cli) (swoole/swow/libevent) or grab one in [lwmbs actions](https://github.com/dixyes/lwmbs/actions)(swow)

To use it, simply concatenate the `micro.sfx` file and any PHP source.

For example: if the content of myawesomeapp.php is

```php
<?php
echo "hello, this is my awesome app." . PHP_EOL;
```

On Linux/macOS:

Note for macOS users: If you've downloaded micro.sfx and macOS does not let you execute it, try:

```bash
sudo xattr -d com.apple.quarantine /path/to/micro.sfx
```

then

```bash
cat /path/to/micro.sfx myawesomeapp.php > myawesomeapp
chmod 0755 ./myawesomeapp
./myawesomeapp
# shows "hello, this is my awesome app."
```

or Windows:

```batch
COPY /b \path\to\micro.sfx + myawesomeapp.php myawesomeapp.exe
myawesomeapp.exe
REM shows "hello, this is my awesome app."
```

## Build micro.sfx

### Preparation

1.Clone this repository into `sapi/micro` under the PHP source directory

```bash
# prepare PHP source
git clone --branch 'PHP-choose-a-release' https://github.com/php/php-src/ php-src
cd php-src
# at PHP source dir
git clone <url for this repo> sapi/micro
```

2.Apply patches

Patches are placed in the "patches" directory. Choose patch(es) as you like, see [Readme.md](patches/Readme.md) in the patches dir for detail

Apply a patch:

```bash
# at PHP source dir
patch -p1 < sapi/micro/patches/<name of patch>
```

### UNIX-like Build

0.Prepare the build environment according to [the official PHP documents](https://www.php.net/manual/en/install.unix.php).

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

At Linux libc compatibility can be a problem, micro provides two kinds of `configure` arguments:

- `--enable-micro=yes`or`--enable-micro`: this will make PIE shared ELF micro sfx, this kind of binary cannot be invoked cross libc (i.e. you cannot run such a binary which was built on alpine with musl on any glibc-based CentOS), but the binary can do ffi and PHP `dl()` function.
- `--enable-micro=all-static`: this will make full static ELF micro sfx, this kind of binary can even run barely on top of any linux kernel, but ffi/`dl()` is not supported.

3.make

```bash
# at PHP source dir
make micro
```

(`make all`(aka. `make`) may work also, but build only the micro SAPI is recommended.)

The built file will be located at sapi/micro/micro.sfx.

### Windows Build

0.Prepare the build environment according to [the official PHP documents](https://wiki.php.net/internals/windows/stepbystepbuild_sdk_2).

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
Due to PHP build system on Windows lack of the ability to statically build PHP binary, you cannot build micro with `nmake`

```batch
# at PHP source dir
nmake micro
```

That built file is at `<arch name like x64>\\<configuration like Release>\\micro.sfx`.

## Optimizations

Hugepages optimization for Linux in the PHP build system results in a huge size of sfx, if you do not take advantage of hugepages, use `disable_huge_page.patch` to shrink the sfx size.

Statically build under Linux needs libc, the most common glibc may be large, musl is recommended instead. manually installed musl or some distros provided musl will provide `musl-gcc` or `musl-clang` wrapper, use one of them before configure by specify CC/CXX environs, for example

```bash
# ./buildconf things...
export CC=musl-gcc
export CXX=musl-gcc
# ./configure things
# make things
```

We hope all dependencies are statically linked into sfx. However, some distro does not provide static versions of them. We may manually build them.

libffi for example (note that the ffi extension is not supported in `all-static` builds):

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
# ./configure things
# make things
```

## Some details

### INI settings

See wiki：[INI-settings](https://github.com/easysoft/phpmicro/wiki/INI-settings)(TODO: en version)

### PHP_BINARY constant

In micro, `PHP_BINARY` is an empty string. You can modify it using an ini setting: `micro.php_binary=somestring`

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
