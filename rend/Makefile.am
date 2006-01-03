lib_LTLIBRARIES = libasa.la
libasa_la_SOURCES = ssa_rend.c
libasa_la_LIBADD = ../font/libasa_font.a ../ssa/libasa_ssa.la $(FREETYPE_LIBS)
libasa_la_LDFLAGS = -version-info 0:0:0
libasa_la_CFLAGS = $(FREETYPE_CFLAGS)
AM_CPPFLAGS = -D__USER__="\"`whoami` at `hostname`\""
