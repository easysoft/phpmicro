# micro 自执行SAPI

[![English readme](https://img.shields.io/badge/README-English%20%F0%9F%87%AC%F0%9F%87%A7-white)](Readme.EN.md)
![php](https://img.shields.io/badge/php-8.0--8.2-royalblue.svg)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![tests](https://github.com/dixyes/phpmicro/actions/workflows/tests.yml/badge.svg)](https://github.com/dixyes/phpmicro/actions/workflows/tests.yml)

micro自执行SAPI提供了php“自执行文件”的可能性

你只需要将构建的micro.sfx文件与任意php文件或者phar包拼接（`cat`或者`copy /b`）为一个文件就可以直接执行这个php文件

## 兼容性

目前兼容PHP8+；兼容Windows、Linux、macOS （可能也支持一些BSDs）。

## 获取与使用

Github actions中构建了一个很少扩展的最小micro，如果需要扩展，请自行构建，参考下方构建说明或使用[crazywhalecc/static-php-cli](https://github.com/crazywhalecc/static-php-cli)（swow/swoole/libevent）从[lwmbs的workflow](https://github.com/dixyes/lwmbs/actions)（swow）下一个

将micro.sfx和php文件拼接即可使用

例如：myawesomeapp.php内容为

```php
<?php
echo "hello, this is my awesome app." . PHP_EOL;
```

linux/macOS下

注意：在macOS下如果你的micro.sfx是下载来的，运行时提示无法打开，可能需要执行下

```bash
sudo xattr -d com.apple.quarantine /path/to/micro.sfx
```

然后

```bash
cat /path/to/micro.sfx myawesomeapp.php > myawesomeapp
chmod 0755 ./myawesomeapp
./myawesomeapp
# 回显 "hello, this is my awesome app."
```

或者Windows下

```batch
COPY /b \path\to\micro.sfx + myawesomeapp.php myawesomeapp.exe
myawesomeapp.exe
REM 回显 "hello, this is my awesome app."
```

## 构建

### 准备源码

1.将本仓库clone到php源码的sapi/micro下

```bash
# 在php源码目录下
git clone <url for this repo> sapi/micro
```

2.打patch

patch文件在patches目录下，选择需要的patch文件，详细作用参考patches下的[Readme.md](patches/Readme.md)

并分别进行patch：

```bash
# 在php源码目录下
patch -p1 < sapi/micro/patches/<name of patch>
```

### unix-like 构建

0.参考官方构建说明准备PHP构建环境

1.buildconf

```bash
# 在php源码目录下
./buildconf --force
```

2.configure

```bash
# 在php源码目录下
./configure <options>
```

参考的选项：

`--disable-phpdbg --disable-cgi --disable-cli --disable-all --enable-micro --enable-phar --with-ffi --enable-zlib`

Linux下，存在C库兼容性问题，对于这个，micro构建系统提供了两种选项：

- `--enable-micro=yes`或者`--enable-micro`：这将会构建PIE的动态的micro，这种micro不能跨C库调用（即在alpine上构建的使用musl的micro不能在只安装了glibc的CentOS上使用，反过来也不能），但支持ffi和PHP的`dl()`函数。
- `--enable-micro=all-static`：这将会构建静态的micro，这种micro不依赖C库，可以直接跑在支持的Linux内核上，但不能使用ffi/`dl()`

3.make

```bash
# 在php源码目录下
make micro
```

（`make all`（或者`make`） 或许也可以，但建议还是只构建micro SAPI

生成的文件在 sapi/micro/micro.sfx

### Windows 构建

0.参考[官方构建说明](https://wiki.php.net/internals/windows/stepbystepbuild_sdk_2)准备PHP构建环境，或者用[我的脚本](https://github.com/dixyes/php-dev-windows-tool)准备一下

1.buildconf

```batch
# 在php源码目录下
buildconf
```

2.configure

```batch
# 在php源码目录下
configure <options>
```

参考的选项：

`--disable-all --disable-zts --enable-micro --enable-phar --with-ffi --enable-zlib`

3.make
由于构建系统的实现问题， Windows下不能使用nmake命令直接构建，使用nmake micro来构建<!--TODO: 现在可以了，抽空改了-->

```batch
# 在php源码目录下
nmake micro
```

生成的文件在 `<架构名>\\<配置名>\\micro.sfx`

## 优化

linux下php对于hugepages优化导致了生成的文件很大，如果不考虑对hugepages的优化，使用disable_huge_page.patch来来减小文件尺寸

linux下静态构建需要包含c标准库，常见的glibc较大，推荐使用musl，手动安装的musl或者某些发行版会提供gcc（或clang）的musl wrapper：`musl-gcc`或者`musl-clang`。在进行configure之前，通过指定CC和CXX变量来使用这些wrapper

例如

```bash
# ./buildconf things...
export CC=musl-gcc
export CXX=musl-gcc
# ./configure balabala
# make balabala
```

linux下构建时一般希望是纯静态的，但构建使用的发行版不一定提供依赖的库（zlib libffi等）的静态库版本，这时考虑自行构建依赖库

以libffi为例（`all-static`构建时不支持ffi）：

```bash
# 通过git获取源码
git clone https://github.com/libffi/libffi
cd libffi
git checkout <version you like, v3.3 for example>
autoreconf -i
# 或者直接下载tarball解压
wget <url>
tar xf <tar name>
cd <extracted name>
# 如果使用musl的话
export CC=musl-gcc
export CXX=musl-gcc
# 构建安装
./configure --prefix=/my/prefered/path &&
make -j`nproc` &&
make install
```

然后使用以下export命令来构建micro：

```bash
# ./buildconf things...
# export CC=musl-xxx things...
export PKG_CONFIG_PATH=/my/prefered/path/lib/pkgconfig
# ./configure balabala
# make balabala
```

## 一些细节

### INI配置

见wiki：[INI-settings](https://github.com/easysoft/phpmicro/wiki/INI-settings)

### PHP_BINARY常量

micro中这个常量是空字符串，可以通过ini配置：`micro.php_binary=somestring`

## 开源许可

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

