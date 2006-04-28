AM_CPPFLAGS = -D__USER__="\"`whoami` at `hostname`\"" -I$(top_srcdir)/include

SUBDIRS = amd64
if BUILD_AMD64
ASM = amd64/commit.o
endif
amd64/commit.o: $(srcdir)/amd64/commit.asm
	$(YASM) -f elf -g stabs -a x86 -m amd64 -o $@ $^

lib_LTLIBRARIES = libasa.la
libasa_la_SOURCES = \
	asa_core.c \
	asa_font.c \
	assa.c \
	avl.c \
	spanproc.c \
	ssa_errtexts.c \
	ssa_frag.c \
	ssa_glyphload.c \
	ssa_lexer.c \
	ssa_run.c \
	ssa_vm.c \
	ssa_wrap.c \
	utf8.c


libasa_la_LIBADD = $(ASM) $(FREETYPE_LIBS)
libasa_la_LDFLAGS = -version-info 0:0:0
libasa_la_CFLAGS = $(FREETYPE_CFLAGS)
 
