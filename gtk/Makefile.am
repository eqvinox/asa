EXTRA_DIST = asa.glade

pkgdatadir = $(datadir)/asa
pkgdata_DATA = asa.glade

noinst_HEADERS = inline_pngs.h asaerrdisp.h

bin_PROGRAMS = gasa
gasa_SOURCES = asaerrdisp.c gasa.c
gasa_LDADD = ../lib/libasa.la $(GTK_LIBS) $(FREETYPE_LIBS)
gasa_LDFLAGS = -export-dynamic
gasa_CFLAGS = $(GTK_CFLAGS) $(FREETYPE_CFLAGS) -DASA_GLADE="\"$(pkgdatadir)/asa.glade\""
gasa_CPPFLAGS = -I$(top_srcdir)/include

