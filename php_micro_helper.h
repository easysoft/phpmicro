#ifndef _PHP_MICRO_DLHELPER_H
#define _PHP_MICRO_DLHELPER_H

#include "php.h"
#include "php_micro.h"

#ifdef _DEBUG
int micro_init(void);
MICRO_SFX_EXPORT int dbgprintf(const char * fmt, ...);
#else
#define dbgprintf(...)
#endif

PHP_FUNCTION(micro_update_extension_dir);
PHP_FUNCTION(micro_enum_modules);
#endif
