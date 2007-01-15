AM_CPPFLAGS = -D__USER__="\"`whoami` at `hostname`\"" -I$(top_srcdir)/include

SUBDIRS = amd64 i686
if BUILD_AMD64
ASM = amd64/commit.o
endif
if BUILD_I686
# ASM = ...
endif
CLEANFILES = $(ASM)

amd64/commit.o: $(srcdir)/amd64/commit.asm
	$(YASM) -f elf -g stabs -a x86 -m amd64 -o $@ $^
i686/%.o: $(srcdir)/i686/%.asm
	$(NASM) -f elf -o $@ $^

lib_LTLIBRARIES = libasa.la
libasa_la_SOURCES = \
	asa_core.c \
	asa_errtexts.c \
	asa_font.c \
	assa.c \
	spanproc.c \
	ssa_anim.c \
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
 
