## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.5 gnits 
# no-dependencies

noinst_LIBRARIES = libfind.a

libfind_a_SOURCES =
EXTRA_DIST = modetype.h wait.h extendbuf.h savedirinfo.h buildcmd.h
BUILT_SOURCES =
SUFFIXES =
MOSTLYCLEANFILES =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
INCLUDES = -I$(top_srcdir)/gnulib/lib

libfind_a_SOURCES += modetype.h nextelem.h
libfind_a_SOURCES += listfile.c nextelem.c extendbuf.c buildcmd.c savedirinfo.c

EXTRA_DIST += waitpid.c forcefindlib.c


libfind_a_LIBADD = @FINDLIBOBJS@
libfind_a_DEPENDENCIES = @FINDLIBOBJS@
# libfind_la_LIBADD = @LTFINDLIBOBJS@

# libfind_a_OBJECTS  += @FINDLIBOBJS@
# libfind_la_OBJECTS += @LTFINDLIBOBJS@
