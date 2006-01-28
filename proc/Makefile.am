if BUILD_AMD64
ASM = amd64/commit.o
endif

SUBDIRS = amd64
lib_LTLIBRARIES = libasa_proc.la
libasa_proc_la_SOURCES = spanproc.c asa_core.c assa.c ssa_run.c ssa_glyphload.c
libasa_proc_la_LIBADD = ../font/libasa_font.a ../ssa/libasa_ssa.la ../vm/libasa.la  $(FREETYPE_LIBS) $(ASM)
libasa_proc_la_LDFLAGS = -version-info 0:0:0
libasa_proc_la_CFLAGS = $(FREETYPE_CFLAGS)
AM_CPPFLAGS = -D__USER__="\"`whoami` at `hostname`\"" -I$(top_srcdir)/include

amd64/commit.o: $(srcdir)/amd64/commit.asm
	$(YASM) -f elf -g stabs -a x86 -m amd64 -o $@ $^

