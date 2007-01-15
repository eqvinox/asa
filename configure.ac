AC_PREREQ(2.57)
AC_INIT(asa, 0.3.2)
AC_CONFIG_AUX_DIR(ac)
AM_INIT_AUTOMAKE
AM_GNU_GETTEXT([external])
AC_CONFIG_HEADER([include/acconf.h])
AC_PREFIX_DEFAULT(~)

# stuff that gets undefined...
asa_gtk_ok=yes
asa_optimize=NONE

asa_opt_disp="none - renderer slower than a snail on slow-motion"
asa_gtk_disp=no


# Checks for programs.
AC_PROG_CC

# stuff that may fail => do it early
AC_MSG_NOTICE([--- checking basic system dependencies ---])
AC_CHECK_HEADERS([endian.h],,[
	AC_MSG_WARN([missing endian.h, defaulting to little endian])
])
AC_CHECK_HEADERS([wchar.h],,[
	AC_MSG_FAILURE([asa needs wide character support, find a wchar.h])
])
AC_MSG_NOTICE([--- checking major library dependencies ---])
AC_ARG_ENABLE([gtk],
	AC_HELP_STRING([--disable-gtk], [do not build the Gtk+-2.0 GUI frontends]),
	[	if test "$enableval" == "no"
		then	unset asa_gtk_ok
			asa_gtk_disp="disabled by parameter"
		fi])

	AC_MSG_NOTICE([note: freetype2 versions earlier than 2.1.10 contain various bugs])
	PKG_CHECK_MODULES([FREETYPE], freetype2 >= 9.8.3 fontconfig >= 2.2.0,, [
		AC_MSG_FAILURE([asa needs freetype2 2.1.10])
	])

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

if test -n "$asa_gtk_ok"
then	PKG_CHECK_MODULES([GTK], gtk+-2.0 >= 2.4.0 glib-2.0 >= 2.4.0 libglade-2.0 >= 2.4.0,, [
		AC_MSG_WARN([asa GUI frontends disabled due to lack of Gtk+-2.0])
		unset asa_gtk_ok
	])
fi

AC_CHECK_HEADERS([png.h])
AC_CHECK_LIB([z], [inflate])
AC_CHECK_LIB([m], [pow])
AC_CHECK_LIB([png], [png_init_io])

AM_ICONV
LDFLAGS="$LDFLAGS $LTLIBICONV"

AC_MSG_NOTICE([--- done checking special dependencies ---])

AC_PROG_RANLIB
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
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup strerror strtoul])

#eval cfgfile="${sysconfdir}/asa.conf"
#AC_DEFINE_UNQUOTED([CONFIG_FILE], ["${cfgfile}"], [default configuration file])

if test -n "$asa_gtk_ok"
then	AC_DEFINE([ASA_BUILD_GTKGUI], 1, [build Gtk GUIs (have Gtk+2.0)])
	asa_gtk_disp=yes
fi
AM_CONDITIONAL([BUILD_GTKGUI], [test -n "$asa_gtk_ok"])
if test "$asa_optimize" == "NONE"
then	AC_DEFINE([ASA_OPT_NONE], 1, [no optimization for asa])
fi
AM_CONDITIONAL([BUILD_AMD64], [test "$asa_optimize" == "AMD64"])
AM_CONDITIONAL([BUILD_I686], [test "$asa_optimize" == "i686"])

echo " "
echo " configuration summary: "
echo " "
echo "   build renderer & ssa libs: always built"
echo "     using optimization:      $asa_opt_disp"
echo "   build Gtk+-2.0 GUIs:       $asa_gtk_disp"
echo " "

AC_CONFIG_FILES([asa.pc])

AC_OUTPUT([
	ac/Makefile
	include/Makefile
	lib/Makefile
	lib/amd64/Makefile
	lib/i686/Makefile
	interfaces/Makefile
	gtk/Makefile
	check/Makefile
	po/Makefile.in
	Makefile
])

