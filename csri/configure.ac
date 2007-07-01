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

AC_CHECK_HEADERS([png.h])
AC_CHECK_LIB([z], [inflate])
AC_CHECK_LIB([m], [pow])
AC_CHECK_LIB([png], [png_init_io])

AC_CPP_PRAGMA_ONCE
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
AC_GCC_VISIBILITY([internal])

AC_PATH_XTRA
platform=""
case $host_os in
mingw*)	# LoadLibraryEx + GetProcAddress
	platform="win32"
	;;
*)	AC_SEARCH_LIBS([dlopen], [dl], [], [
		AC_MSG_FAILURE([dlopen not found - unsupported dynamic loader architecture / operating system])
	])
esac

AM_CONDITIONAL([BUILD_MINGW], [test "$platform" == "win32" ])


# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([stdlib.h string.h sys/ioctl.h sys/time.h sys/wait.h sys/mman.h unistd.h libgen.h getopt.h])
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
AC_CHECK_FUNCS([memset strdup strerror strtoul])

csri_path="\${libdir}/csri:~/.csri/lib"
AC_ARG_WITH(csri-libpath,
	AC_HELP_STRING([--with-csri-libpath=PATH1:PATH2],
		[Look for CSRI renderers in the specified directories. Separate directory names with colons. Tildes are expanded at run-time. [[$libdir/csri:~/.csri/lib]]]),
[
    if test "$withval" != "yes" -a "$withval" != "" -a "$withval" != "no"
    then csri_path="$withval"
    fi
])
AC_SUBST([csri_path], ["$csri_path"])

AC_CONFIG_FILES([csri.pc])

echo ""
echo "using search path: ${csri_path}"
echo ""
AC_OUTPUT([
	ac/Makefile
	include/Makefile
	subhelp/Makefile
	lib/Makefile
	frontends/Makefile
	frontends/avisynth25/Makefile
	frontends/cmdline/Makefile
	backends/Makefile
	Makefile
])

