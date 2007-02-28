AC_PREREQ(2.57)
AC_INIT(csri, 0.1.0)
AC_CONFIG_AUX_DIR(ac)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER([include/acconf.h])

# Checks for programs.
AC_PROG_CC

AC_PROG_LIBTOOL
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

AC_PROG_INSTALL

AC_MSG_CHECKING([[whether preprocessor supports #pragma once]])
AC_PREPROC_IFELSE(
	[AC_LANG_PROGRAM([[#pragma once]])],
	[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_PRAGMA_ONCE], [1], [Preprocessor support for #pragma once])
	],
	[AC_MSG_RESULT([no])])

AC_DEFUN([AC_C_FLAG], [{
	AC_LANG_PUSH(C)
	ac_c_flag_save="$CFLAGS"
	CFLAGS="$CFLAGS $1"
	AC_MSG_CHECKING([[whether $CC supports $1]])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[]])],
		[AC_MSG_RESULT([yes])],
		[
			CFLAGS="$ac_c_flag_save"
			AC_MSG_RESULT([no])
			$2
		])
	AC_LANG_POP(C)
	}])

AC_C_FLAG([-Wall])
AC_C_FLAG([-Wextra],[AC_C_FLAG([-W])])
AC_C_FLAG([-Wno-unused-parameter])
AC_C_FLAG([-Winvalid-pch])
AC_C_FLAG([-pedantic])
AC_C_FLAG([-std=c99],[AC_C_FLAG([-c99])])
AC_ARG_ENABLE([werror],
	AC_HELP_STRING([--enable-werror], [compile with -Werror (for developers)]),
	[	if test "$enableval" == "yes"
		then	AC_C_FLAG([-Werror])
		fi])


AC_PATH_XTRA

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([stdlib.h string.h sys/ioctl.h sys/time.h sys/wait.h sys/mman.h syslog.h unistd.h libgen.h getopt.h])
AC_CHECK_FUNCS([getopt_long])
#
# Checks for typedefs, structures, and compiler characteristics.
# Checks for library functions.
#AC_FUNC_FORK
#AC_PROG_GCC_TRADITIONAL
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL
AC_FUNC_MEMCMP
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup strerror strtoul])

#eval cfgfile="${sysconfdir}/asa.conf"
#AC_DEFINE_UNQUOTED([CONFIG_FILE], ["${cfgfile}"], [default configuration file])

AC_CONFIG_FILES([csri.pc])

AC_OUTPUT([
	ac/Makefile
	include/Makefile
	subhelp/Makefile
	Makefile
])

