AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = asa.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = asa.pc

if BUILD_RENDERER
REND = font vm proc interfaces
endif
if BUILD_GTKGUI
GTKGUI = gtk
endif
SUBDIRS = ac include ssa $(REND) $(GTKGUI) check
