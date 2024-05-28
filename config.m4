PHP_ARG_ENABLE([micro],,
  [AS_HELP_STRING([--enable-micro],
    [enable building micro PHP sfx ])],
  [no],
  [no])

dnl AC_CHECK_FUNCS(setproctitle)

dnl AC_CHECK_HEADERS([sys/pstat.h])

dnl AC_CACHE_CHECK([for PS_STRINGS], [cli_cv_var_PS_STRINGS],
dnl [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <machine/vmparam.h>
dnl #include <sys/exec.h>
dnl ]],
dnl [[PS_STRINGS->ps_nargvstr = 1;
dnl PS_STRINGS->ps_argvstr = "foo";]])],
dnl [cli_cv_var_PS_STRINGS=yes],
dnl [cli_cv_var_PS_STRINGS=no])])
dnl if test "$cli_cv_var_PS_STRINGS" = yes ; then
dnl   AC_DEFINE([HAVE_PS_STRINGS], [], [Define to 1 if the PS_STRINGS thing exists.])
dnl fi

AC_MSG_CHECKING(for micro build)
if test "$PHP_MICRO" != "no"; then
  AC_MSG_RESULT(yes)

  PHP_ADD_MAKEFILE_FRAGMENT($abs_srcdir/sapi/micro/Makefile.frag)

  dnl Set filename.
  SAPI_MICRO_PATH=sapi/micro/micro.sfx

  dnl Select SAPI.
  dnl CFLAGS="$CFLAGS -DPHP_MICRO_BUILD_SFX"
  PHP_SUBST(MICRO_CFLAGS)
  if test "x${enable_debug##yes}" != "x${enable_debug}"; then
    MICRO_CFLAGS=-D_DEBUG
  fi
  PHP_SELECT_SAPI(micro, program, php_micro.c php_micro_helper.c php_micro_hooks.c php_micro_fileinfo.c, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 \$(MICRO_CFLAGS), '$(SAPI_MICRO_PATH)')
  dnl add cli_???_process_title functions
  PHP_ADD_SOURCES_X(sapi/cli, php_cli_process_title.c ps_title.c, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1, PHP_MICRO_OBJS)

  OVERALL_TARGET="$OVERALL_TARGETS \$(SAPI_MICRO_PATH) \$(MICRO_EXES)"  

  MICRO_LIBS='$(EXTRA_LIBS)'
  case $host_alias in
  *aix*)
    AC_MSG_ERROR(not yet support aix)
    
    if test "$php_sapi_module" = "shared"; then
      BUILD_MICRO="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_MICRO_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/.libs\/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_MICRO_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_MICRO_PATH)"
    else
      BUILD_MICRO="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_MICRO_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_MICRO_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_MICRO_PATH)"
    fi
    ;;
  *darwin*)
    if test "x${PHP_MICRO%%all-static*}" != "x${PHP_MICRO}"; then
      AC_MSG_WARN(macOS donot support static mach-o build)
    fi
    BUILD_MICRO="\$(CC) \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(NATIVE_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_MICRO_OBJS:.lo=.o) \$(PHP_FRAMEWORKS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_MICRO_PATH)"
    ;;
  *)
    if test "x${PHP_MICRO%%all-static*}" != "x${PHP_MICRO}"; then
      EXTRA_LDFLAGS_PROGRAM="$EXTRA_LDFLAGS_PROGRAM -all-static"
    else
      dnl check if cc supports -static-libgcc
      AX_CHECK_COMPILE_FLAG([-static-libgcc], [
        EXTRA_LDFLAGS_PROGRAM="$EXTRA_LDFLAGS_PROGRAM -static-libgcc"
      ], [])
      dnl replace libresolv things with static versions
      MICRO_LIBS='$(EXTRA_LIBS:-lresolv=-Wl,-Bstatic,-lresolv,-Bdynamic)'
    fi
    PHP_SUBST(EXTRA_LDFLAGS)
    BUILD_MICRO="\$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_MICRO_OBJS) ${MICRO_LIBS} \$(ZEND_EXTRA_LIBS) -o \$(SAPI_MICRO_PATH)"
    ;;
  esac

  dnl Set executable for tests.
  dnl PHP_EXECUTABLE="\$(top_builddir)/\$(SAPI_CLI_PATH)"
  dnl PHP_SUBST(PHP_EXECUTABLE)

  dnl Expose to Makefile.
  PHP_SUBST(SAPI_MICRO_PATH)
  PHP_SUBST(BUILD_MICRO)

  dnl PHP_OUTPUT(sapi/cli/php.1)

  dnl PHP_INSTALL_HEADERS([sapi/cli/cli.h])
fi
