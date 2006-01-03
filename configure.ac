AC_PREREQ(2.57)
AC_INIT(asa, 0.3)
AC_CONFIG_AUX_DIR(ac)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER([include/acconf.h])
AC_PREFIX_DEFAULT(~)

# stuff that gets undefined...
asa_rend_ok=yes
asa_gtk_ok=yes

asa_rend_disp=no
asa_gtk_disp=no


AC_CANONICAL_HOST

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
AC_ARG_ENABLE([renderer],
	AC_HELP_STRING([--disable-renderer], [do not build the FreeType2 and FontConfig based renderer]),
	[	if test "$enableval" == "no"
		then	unset asa_rend_ok
			asa_rend_disp="disabled by parameter"
		fi])
AC_ARG_ENABLE([gtk],
	AC_HELP_STRING([--disable-gtk], [do not build the Gtk+-2.0 GUI frontends]),
	[	if test "$enableval" == "no"
		then	unset asa_gtk_ok
			asa_gtk_disp="disabled by parameter"
		fi])
if test -n "$asa_rend_ok"
then	AC_MSG_NOTICE([note that the package versions are almost arbitary, it may work with older ones, it may not work with newer ones...])
	PKG_CHECK_MODULES([FREETYPE], freetype2 >= 9.0.0 fontconfig >= 2.2.0,, [
		AC_MSG_WARN([asa needs FreeType2 and fontconfig to be able to render something - ALL RENDERING DISABLED])
		unset asa_rend_ok
	])
fi
if test -n "$asa_gtk_ok"
then	PKG_CHECK_MODULES([GTK], gtk+-2.0 >= 2.4.0 glib-2.0 >= 2.4.0,, [
		AC_MSG_WARN([asa GUI frontends disabled due to lack of Gtk+-2.0])
		unset asa_gtk_ok
	])
fi
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
AC_DEFUN([AC_CXX_FLAG], [{
	AC_LANG_PUSH(C++)
	ac_cxx_flag_save="$CXXFLAGS"
	CXXFLAGS="$CXXFLAGS $1"
	AC_MSG_CHECKING([[whether $CXX supports $1]])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[]])],
		[AC_MSG_RESULT([yes])],
		[
			CXXFLAGS="$ac_cxx_flag_save"
			AC_MSG_RESULT([no])
			$2
		])
	AC_LANG_POP(C++)
	}])

AC_C_FLAG([-Wall])
AC_C_FLAG([-Wextra],[AC_C_FLAG([-W])])
AC_C_FLAG([-Wno-unused-parameter])
AC_C_FLAG([-Winvalid-pch])
AC_C_FLAG([-pedantic])
AC_C_FLAG([-std=c99],[AC_C_FLAG([-c99])])
AC_CXX_FLAG([-Wall])
AC_CXX_FLAG([-Wextra],[AC_CXX_FLAG([-W])])
AC_CXX_FLAG([-Wno-unused-parameter])
AC_CXX_FLAG([-Winvalid-pch])
AC_CXX_FLAG([-Wno-long-long])
AC_CXX_FLAG([-pedantic])
AC_CXX_FLAG([-std=c++98])


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
AC_CHECK_HEADERS([stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h syslog.h unistd.h getopt.h])
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

if test -n "$asa_rend_ok"
then	AC_DEFINE([ASA_BUILD_RENDERER], 1, [build renderer (have freetype, fontconfig)])
	asa_rend_disp=yes
fi
AM_CONDITIONAL([BUILD_RENDERER], [test -n "$asa_rend_ok"])
if test -n "$asa_gtk_ok"
then	AC_DEFINE([ASA_BUILD_GTKGUI], 1, [build Gtk GUIs (have Gtk+2.0)])
	asa_gtk_disp=yes
fi
AM_CONDITIONAL([BUILD_GTKGUI], [test -n "$asa_gtk_ok"])

echo " "
echo " configuration summary: "
echo " "
echo "   build ssa libs:       always built"
echo "   build renderer:       $asa_rend_disp"
echo "   build Gtk+-2.0 GUIs:  $asa_gtk_disp"
echo " "

AC_OUTPUT([ac/Makefile include/Makefile ssa/Makefile font/Makefile vm/Makefile proc/Makefile gtk/Makefile gtk/include/Makefile gtk/common/Makefile gtk/gcheck/Makefile gtk/viewer/Makefile check/Makefile Makefile])

