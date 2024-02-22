
# 补丁 / Patches

名称 Name | 平台 Platform | 可选? Optional? | 用途 Usage
--- | --- | --- | ---
phar_\<php version\>.patch | * | 可选 Optional | 允许micro使用压缩phar Allow micro use compressed phar
static_opcache_\<php version\>.patch | * | 可选 Optional | 支持静态构建opcache Support build opcache statically
macos_iconv.patch | macOS | 可选 Optional | 支持链接到系统的iconv Support link against system iconv
static_extensions_win32_\<php version\>.patch | Windows | 可选 Optional | 支持静态构建Windows其他扩展 Support build other extensions for windows
cli_checks_\<php version\>.patch | * | 可选 Optional | 修改PHP内核中硬编码的SAPI检查 Modify hardcoden SAPI name checks in PHP core
disable_huge_page.patch | Linux | 可选 Optional | 禁用linux构建的max-page-size选项，缩减sfx体积（典型的， 10M+ -> 5M） Disalbe max-page-size for linux build，shrink sfx size (10M+ -> 5M typ.)
vcruntime140_\<php version\>.patch | Windows | 必须 Nessesary | 禁用sfx启动时GetModuleHandle(vcruntime140(d).dll) Disable GetModuleHandle(vcruntime140(d).dll) at sfx start
win32_\<php version\>.patch | Windows | 必须 Nessesary | 修改构建系统以静态构建 Modify build system for build sfx file
zend_stream.patch | Windows | 必须 Nessesary | 修改构建系统以静态构建 Modify build system for build sfx file
comctl32.patch | Windows | 可选 Optional | 添加comctl32.dll manifest以启用[visual style](https://learn.microsoft.com/en-us/windows/win32/controls/visual-styles-overview) (会让窗口控件好看一些) Add manifest dependency for comctl32 to enable [visual style](https://learn.microsoft.com/en-us/windows/win32/controls/visual-styles-overview) (makes window control looks modern)

## Something special

### phar.patch

这个patch绕过PHAR对micro的文件名中包含".phar"的限制（并不会允许micro本身以外的其他文件），这使得micro文件名中不含".phar"时依然可以使用压缩过的phar

This patch bypasses the limit that PHAR must contains ".phar" in their file name when invoke with micro (it will not allow files other than the sfx to be regarded as phar), this makes micro can handle compressed phar without a custom stub.

有stub的PHAR不需要这个补丁也可以使用

phar with a stub (may be a special one) do not need this patch.

这个补丁只能在micro中使用，会导致其他SAPI编译不过

This patch can only be used with micro, it makes other SAPIs failed to build.

### static_opcache

静态链接opcache到PHP里，可以在其他的SAPI上用

Make opcache statically link into PHP, can be used for other SAPIs.

### cli_checks

绕过许多“是不是cli”的检查

Bypass many cli SAPI name checks.

### cli_static

允许Windows的cli静态构建，不是给micro用的

Make Windows cli SAPI able to built full-statically, not a patch for micro
