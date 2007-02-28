## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.5 gnits 
# no-dependencies

noinst_LIBRARIES = libfind.a

if CROSS_COMPILING
# The regexprops program needs to be a native executable, so we 
# can't build it with a cross-compiler.
else
noinst_PROGRAMS = regexprops
regexprops_SOURCES = regexprops.c regextype.c
endif

libfind_a_SOURCES = gnulib-version.c
EXTRA_DIST = modetype.h wait.h extendbuf.h savedirinfo.h buildcmd.h \
	gnulib-version.h gnulib-version.sh gnulib-version.c
BUILT_SOURCES = gnulib-version.c
SUFFIXES =
MOSTLYCLEANFILES = 
CLEANFILES = 
DISTCLEANFILES =
MAINTAINERCLEANFILES =
IMPORT_GNULIB_CONFIG = $(top_srcdir)/import-gnulib.config

INCLUDES = -I../gnulib/lib -I$(top_srcdir)/gnulib/lib 
LDADD = ../gnulib/lib/libgnulib.a @INTLLIBS@

libfind_a_SOURCES += modetype.h nextelem.h printquoted.h listfile.h regextype.h
libfind_a_SOURCES += listfile.c nextelem.c extendbuf.c buildcmd.c savedirinfo.c forcefindlib.c qmark.c printquoted.c regextype.c

EXTRA_DIST += waitpid.c forcefindlib.c


libfind_a_LIBADD = @FINDLIBOBJS@
libfind_a_DEPENDENCIES = @FINDLIBOBJS@
# libfind_la_LIBADD = @LTFINDLIBOBJS@

# libfind_a_OBJECTS  += @FINDLIBOBJS@
# libfind_la_OBJECTS += @LTFINDLIBOBJS@

$(srcdir)/gnulib-version.c: $(srcdir)/gnulib-version.sh $(IMPORT_GNULIB_CONFIG)
	sh $(srcdir)/gnulib-version.sh $(top_srcdir) $(srcdir)  $(IMPORT_GNULIB_CONFIG)