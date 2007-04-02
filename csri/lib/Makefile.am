lib_LTLIBRARIES = libcsri.la
noinst_HEADERS = csrilib.h posix/csrilib_os.h
libcsri_la_SOURCES = list.c wrap.c posix/enumerate.c
libcsri_la_CFLAGS = -I$(top_srcdir)/lib/posix -I$(top_srcdir)/include
libcsri_la_LIBADD = ../subhelp/libsubhelp_la-logging.lo -ldl
libcsri_la_LDFLAGS = -static

