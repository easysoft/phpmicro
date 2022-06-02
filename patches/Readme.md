
# 补丁 / Patches

名称 Name | 平台 Platform | 可选? Optional? | 用途 Usage
--- | --- | --- | ---
static_opcache_\<php version\>.patch | * | 可选 Optional | 支持静态构建opcache Support build opcache statically
cli_checks_\<php version\>.patch | * | 可选 Optional | 修改PHP内核中硬编码的SAPI检查 Modify hardcoden SAPI name checks in PHP core
disable_huge_page.patch | Linux | 可选 Optional | 禁用linux构建的max-page-size选项，缩减sfx体积（典型的， 10M+ -> 5M） Disalbe max-page-size for linux build，shrink sfx size (10M+ -> 5M typ.)
vcruntime140_\<php version\>.patch | Windows | 必须 Nessesary | 禁用sfx启动时GetModuleHandle(vcruntime140(d).dll) Disable GetModuleHandle(vcruntime140(d).dll) at sfx start
win32_\<php version\>.patch | Windows | 必须 Nessesary | 修改构建系统以静态构建 Modify build system for build sfx file
zend_stream.patch | Windows | 必须 Nessesary | 修改构建系统以静态构建 Modify build system for build sfx file
