if BUILD_RENDERER
RENDLIB = ../vm/libasa.la ../font/libasa_font.a
endif
noinst_PROGRAMS = asacheck
asacheck_SOURCES = asacheck.c
asacheck_LDADD = ../ssa/libasa_ssa.la $(RENDLIB) $(FREETYPE_LIBS)
asacheck_CFLAGS = $(FREETYPE_CFLAGS) -I$(top_srcdir)/include
