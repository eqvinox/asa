if BUILD_MINGW
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/csri/include
lib_LTLIBRARIES = libasa_avs.la
libasa_avs_la_SOURCES = avisynth.cpp
libasa_avs_la_LIBADD = ../lib/libasa.la
libasa_avs_la_LDFLAGS = -version-info 0:0:0 -no-undefined
# need IDL compiler for DirectShow?
endif

EXTRA_DIST=avisynth.h avisynth.cpp directshow.cpp directshow.idl
