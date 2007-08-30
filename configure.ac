AC_PREREQ(2.57)
AC_INIT(asa, 0.3.2)
AC_CONFIG_AUX_DIR(ac)
AM_INIT_AUTOMAKE

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

if test "$build" != "$host"
then	AC_MSG_WARN([cross-compiling will need manual fiddling with lib/imports/import_prep.h])
fi

# okay, so the GNU guys suck. WOW. WHO WOULD HAVE GUESSED?

# this used to be here:
dnl AM_MKINSTALLDIRS
# but it doesn't seem to exist anymore. now, let's run the blame lottery...
#  ... and the WINNER IIIIIISSSS:
AC_DEFUN([AM_INTL_SUBDIR],[])
# yes, this sucks. no, i don't know how to do it right.
#
# if it doesn't work, write a flame to:
# Ulrich "don't build glibc yourself!" Drepper.
# (google for his mail address. he works at RedHat.)

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.5])
AC_CONFIG_HEADER([include/acconf.h])

# stuff that gets undefined...
asa_optimize=NONE

asa_opt_disp="none - renderer slower than a snail on slow-motion"
simpfmt_disp="yes"


# Checks for programs.
AC_PROG_CC
AC_PROG_YACC
AM_PROG_LEX

asa_imports_file="yes"
asa_imports_prep="yes"
AC_ARG_ENABLE([],
	AC_HELP_STRING([--disable-imports-file], [do not support loading import directives from .../share/asa/imports]),
	[	if test "$enableval" == "no"
		then	asa_imports_file="no"
		fi])
AC_ARG_ENABLE([],
	AC_HELP_STRING([--disable-imports-prep], [do not compile import directives into library]),
	[	if test "$enableval" == "no"
		then	asa_imports_prep="no"
		fi])

# stuff that may fail => do it early
AC_MSG_NOTICE([--- checking basic system dependencies ---])
AC_CHECK_HEADERS([endian.h],,[
	AC_MSG_WARN([missing endian.h, defaulting to little endian])
])
AC_CHECK_HEADERS([wchar.h],,[
	AC_MSG_FAILURE([asa needs wide character support, find a wchar.h])
])
AC_MSG_NOTICE([--- checking major library dependencies ---])
PKG_CHECK_MODULES([FREETYPE], freetype2 >= 9.10.3,, [
	AC_MSG_FAILURE([asa needs freetype2 2.2.1 or newer. Also check your pkg-config installation.])
])
PKG_CHECK_MODULES([FONTCONFIG], fontconfig >= 2.2.0,, [
	AC_MSG_FAILURE([asa needs fontconfig 2.2 or newer. Also check your pkg-config installation.])
])
PKG_CHECK_MODULES([PCRE], libpcre >= 6.5,, [
	AC_MSG_WARN([libpcre not available - no support for simple subtitle formats.])
	simpfmt_disp="no - pcre >= 6.5 missing"
])

LDFLAGS="$LDFLAGS $FREETYPE_LIBS $FONTCONFIG_LIBS"
AC_CHECK_FUNCS([FcFini])

	try_optimize=1
	AC_ARG_ENABLE([optimize],
		AC_HELP_STRING([--disable-optimize], [do not try to optimize asa]),
		[	if test "$enableval" == "no"
			then	try_optimize=""
			fi
		])

	if test -n "$try_optimize"
	then	case $host in
		x86_64-*-*)
			AC_CHECK_TOOL(YASM, yasm, "")
			if test -z "$YASM"
			then	AC_MSG_WARN([yasm not found - not using AMD64 optimizations])
			else	asa_optimize="AMD64"
				asa_opt_disp="AMD64 + SSE2"
				AC_DEFINE([ASA_OPT_AMD64], 1, [optimize asa for AMD64])
			fi
		;;
		i686-*-*)
			NASM=""
			AC_CHECK_TOOL(YASM, yasm, "")
			if test -z "$YASM"
			then	AC_CHECK_TOOL(NASM, nasm, "")
				if test -z "$NASM"
				then	AC_MSG_WARN([neither yasm nor nasm found - not using i686 optimizations])
				fi
			else	NASM="$YASM -g stabs"
			fi

			if test "$NASM"
			then	asa_optimize="i686"
				asa_opt_disp="SSE2"
				AC_DEFINE([ASA_OPT_I686], 1, [optimize asa for i686])
			fi
		;;
	esac
	fi

AC_CHECK_LIB([m], [pow])

#AC_CHECK_HEADERS([png.h])
#AC_CHECK_LIB([z], [inflate])
#AC_CHECK_LIB([png], [png_init_io])

AM_ICONV
LDFLAGS="$LDFLAGS $LTLIBICONV"

if test -d "$srcdir/csri"
then	AC_CONFIG_SUBDIRS([csri])
else	AC_CHECK_HEADERS([csri/csri.h],,[
		AC_MSG_FAILURE([neither csri subdirectory nor installed header files found.])
	])
fi

AC_MSG_NOTICE([--- done checking special dependencies ---])

AC_PROG_LIBTOOL
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

AC_PROG_INSTALL

AC_CPP_PRAGMA_ONCE
AC_C_FLAG([-Wall])
AC_C_FLAG([-Wextra],[AC_C_FLAG([-W])])
AC_C_FLAG([-Wno-unused-parameter])
AC_C_FLAG([-Winvalid-pch])
AC_C_FLAG([-Wc++-compat])
AC_C_FLAG([-Wdeclaration-after-statement])
AC_C_FLAG([-Wshadow])
AC_C_FLAG([-Wno-missing-field-initializers],,[wnomissinginit="-Wno-missing-field-initializers"])
AC_SUBST([wnomissinginit])
AC_C_FLAG([-pedantic])
if test -n "$try_optimize"
then	AC_C_FLAG([-funroll-loops])
	AC_C_FLAG([-ffast-math])
fi
AC_C_FLAG([-std=c99],[AC_C_FLAG([-c99])])
AC_ARG_ENABLE([werror],
	AC_HELP_STRING([--enable-werror], [compile with -Werror (for developers)]),
	[	if test "$enableval" == "yes"
		then	AC_C_FLAG([-Werror],,[werror="-Werror"])
		fi])
AC_SUBST([werror])
AC_GCC_VISIBILITY([internal])


AC_PATH_XTRA

# Checks for libraries.
#AC_ARG_WITH([lwres],
#	AC_HELP_STRING([--with-lwres],
#	               [use lightweight resolver support (default is NO, don't enable unless you use lwresd)]),
#	[
#		AC_CHECK_LIB([lwres], [lwres_getaddrinfo],,[
#			AC_MSG_FAILURE([bind lightweight resolver not usable])
#		])
#		AC_DEFINE([RES_LWRES], [], [use lightweight resolver])
#	], [
#		AC_SEARCH_LIBS([getaddrinfo], [nsl],,[AC_MSG_FAILURE([getaddrinfo support missing])])
#	])
#
#AC_SEARCH_LIBS([SHA1], [ssl crypto],,[AC_MSG_FAILURE([SHA1 support missing])])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h sys/mman.h syslog.h unistd.h getopt.h])
AC_CHECK_HEADERS([arpa/inet.h malloc.h netdb.h netinet/in.h netinet/ip.h libgen.h getopt.h])
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
AC_CHECK_FUNCS([memset _strdup strdup strerror strtoul])
AC_CHECK_DECLS([_strdup])
AC_CHECK_DECLS([strdup])

#eval cfgfile="${sysconfdir}/asa.conf"
#AC_DEFINE_UNQUOTED([CONFIG_FILE], ["${cfgfile}"], [default configuration file])

platform=""
case $host_os in
mingw*)	platform="win32"
	;;
esac
AM_CONDITIONAL([BUILD_MINGW], [test "$platform" == "win32" ])

if test "$asa_optimize" == "NONE"
then	AC_DEFINE([ASA_OPT_NONE], 1, [no optimization for asa])
fi
AM_CONDITIONAL([BUILD_AMD64], [test "$asa_optimize" == "AMD64"])
AM_CONDITIONAL([BUILD_I686], [test "$asa_optimize" == "i686"])
AM_CONDITIONAL([IMPORTS_FILE], [test "$asa_imports_file" == "yes"])
AM_CONDITIONAL([IMPORTS_PREP], [test "$asa_imports_prep" == "yes"])

csrilibdir="${libdir}/csri"
AC_SUBST([csrilibdir], [$csrilibdir])

AC_CONFIG_FILES([asa.pc])

AC_OUTPUT([
	ac/Makefile
	include/Makefile
	lib/Makefile
	lib/amd64/Makefile
	lib/i686/Makefile
	lib/import/Makefile
	interfaces/Makefile
	po/Makefile.in
	Makefile
])

echo " "
echo " configuration summary: "
echo " "
echo "   library installation path: $csrilibdir"
echo "   build renderer & ssa libs: always built"
echo "   simple formats (like SRT): $simpfmt_disp"
echo "          using optimization: $asa_opt_disp"
echo " "

