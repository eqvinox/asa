AUTOMAKE_OPTIONS = foreign

if BUILD_RENDERER
REND = font vm proc interfaces
endif
if BUILD_GTKGUI
GTKGUI = gtk
endif
SUBDIRS = ac include ssa $(REND) $(GTKGUI) check
