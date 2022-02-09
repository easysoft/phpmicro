# micro 自执行SAPI

micro自执行SAPI提供了php“自执行文件”的可能性

你只需要将构建的micro.sfx文件与任意php文件或者phar包拼接（cat或者copy /b）为一个文件就可以直接执行这个php文件

# 兼容性

目前兼容PHP8+；兼容Windows、Linux、macOS。

# 构建

## 准备源码

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

## unix-like 构建

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

3.make

```bash
# 在php源码目录下
make micro
```

（`make all`（或者`make`） 或许也可以，但建议还是只构建micro SAPI

生成的文件在 sapi/micro/micro.sfx

## Windows 构建

0.参考官方构建说明准备PHP构建环境

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
由于构建系统的实现问题， Windows下不能使用nmake命令直接构建，使用nmake sfx来构建

```batch
# 在php源码目录下
nmake sfx
```

生成的文件在 `<架构名>\\<配置名>\\micro.sfx`

# 使用

将micro.sfx和php文件拼接即可

例如：myawesomeapp.php内容为

```php
<?php
echo "hello, this is my awesome app." . PHP_EOL;
```

linux下

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

# 优化

linux下php对于hugepages优化导致了生成的文件很大，如果不考虑对hugepages的优化，使用disable_huge_page.patch来来减小文件尺寸

linux下静态构建需要包含c标准库，常见的glibc较大，推荐使用musl，手动安装的musl或者某些发行版会提供gcc（或clang）的musl wrapper：musl-gcc或者musl-clang。在进行configure之前，通过指定CC和CXX变量来使用这些wrapper

例如

```bash
# ./buildconf things...
export CC=musl-gcc
export CXX=musl-gcc
# ./configure balabala
# make balabala
```

linux下构建时一般希望是纯静态的，但构建使用的发行版不一定提供依赖的库（zlib libffi等）的静态库版本，这时考虑自行构建依赖库

以libffi为例：

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

# 开源许可

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
