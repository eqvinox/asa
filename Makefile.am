AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = asa.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = asa.pc

if BUILD_GTKGUI
GTKGUI = gtk
endif
SUBDIRS = ac include lib $(GTKGUI) check
