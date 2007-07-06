AM_YFLAGS = -d
AM_CPPFLAGS = -D_GNU_SOURCE \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/csri/include

noinst_HEADERS = imports.h
noinst_LTLIBRARIES = libimport.la
libimport_la_SOURCES = imports_par.y imports_lex.l imports_vm.c

# force dependency
imports_lex.lo: imports_par.h

