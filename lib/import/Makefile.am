if IMPORTS_PREP
noinst_PROGRAMS = preparse
preparse_LDFLAGS = -limportparser

BUILT_SOURCES = imports_prep.h
imports_prep.h: $(srcdir)/imports libimportparser.la preparse
	./preparse $(srcdir)/imports > imports_prep.h

IMPORTS_PREP = -DIMPORTS_PREP
else
IMPORTS_PREP =
endif

if IMPORTS_FILE
IMPORTS_FILE = "-DIMPORTS=\"$(pkgdatadir)/imports\""
IMPORTS_FILE_LIB = -limportparser
dist_pkgdata_DATA = imports
else
IMPORTS_FILE =
IMPORTS_FILE_LIB =
endif

AM_YFLAGS = -d
AM_CPPFLAGS = -D_GNU_SOURCE \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/csri/include
AM_CFLAGS = $(PCRE_CFLAGS) $(IMPORTS_PREP) $(IMPORTS_FILE) $(wnomissinginit)

noinst_HEADERS = imports.h
noinst_LTLIBRARIES = libimport.la libimportparser.la
libimportparser_la_SOURCES = imports_par.y imports_lex.l
libimportparser_la_LIBADD = $(PCRE_LIBS) $(LEXLIB)
libimport_la_SOURCES = imports_vm.c
libimport_la_LIBADD = $(IMPORTS_FILE_LIB)

