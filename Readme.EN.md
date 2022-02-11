# micro self-executable SAPI for PHP

micro self-executable SAPI make PHP self-executable.

Just concat micro.sfx and random php source or phar into single file to use it.

# Compatiable

Yet only support PHP8+; Windows,Linux,macOS.

# Build micro.sfx

## Preparation

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

## UNIX-like Build

0.Prepare build environment according to offical PHP documents.

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

3.make

```bash
# at PHP source dir
make micro
```

(`make all`(aka. `make`) may work also, but only build micro SAPI -s recommended.

That built file is located at sapi/micro/micro.sfx.

## Windows Build

0.Prepare build environment according to offical PHP documents.

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
Due to PHP build system on Windows lack of ablity to statically build PHP binary, you cannot build micro with `nmake`

```batch
# at PHP source dir
nmake micro
```

That built file is located at `<arch name like x64>\\<configuration like Release>\\micro.sfx`.

# Usage

Just concatenate micro.sfx and php source.

For example：if contents of myawesomeapp.php is

```php
<?php
echo "hello, this is my awesome app." . PHP_EOL;
```

at linux:

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

# Optimizations

Hugepages optimization for linux in PHP build system insults huge size of sfx, if you donot take advantage of hugepages, use disable_huge_page.patch to shrink sfx size.

Statically build under linux needs libc, most common glibc may be large, musl is recommended hence. manually installed musl or some distros provided musl will provide `musl-gcc` or `musl-clang` wrapper, use one of them before configure by specify CC/CXX environ, for example

```bash
# ./buildconf things...
export CC=musl-gcc
export CXX=musl-gcc
# ./configure balabala
# make balabala
```
We hope all dependencies is statically linked in sfx, however some distro donot provide static version of them, we may manually build them, the case of libffi
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

# OSS Licese

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

# remind me to update English readme and fix typos and strange or offensive expressions

