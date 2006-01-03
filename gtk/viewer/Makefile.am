noinst_PROGRAMS = gasaview
gasaview_SOURCES = gasaview.c
gasaview_LDADD = ../../ssa/libasa_ssa.la ../common/libasagtk_common.a ../../vm/libasa.la ../../font/libasa_font.a $(GTK_LIBS)
gasaview_CFLAGS = $(GTK_CFLAGS) $(FREETYPE_CFLAGS)
gasaview_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/gtk/include
