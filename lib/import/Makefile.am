AM_YFLAGS = -d
AM_CPPFLAGS = -D_GNU_SOURCE \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/csri/include

noinst_HEADERS = imports.h
noinst_LTLIBRARIES = libimport.la
libimport_la_SOURCES = imports_par.y imports_lex.l imports_vm.c
libimport_la_LIBADD = $(PCRE_LIBS) $(LEXLIB)
libimport_la_CFLAGS = $(PCRE_CFLAGS) "-DIMPORTS=\"$(pkgdatadir)/imports\""

pkgdata_DATA = imports

