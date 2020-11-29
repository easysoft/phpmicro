#ifndef _PHP_MICRO_H
#define _PHP_MICRO_H

#ifdef PHP_WIN32
# define PHP_MICRO_SFX_FILESIZE_ID 12345
# define MICRO_SFX_EXPORT __declspec(dllexport) __declspec(noinline)
#else
# define MICRO_SFX_EXPORT __attribute__((visibility ("default")))
#endif


#endif // _PHP_MICRO_H
