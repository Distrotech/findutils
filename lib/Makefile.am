## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.5 gnits
# no-dependencies
AM_CFLAGS = $(WARN_CFLAGS)

noinst_LIBRARIES = libfind.a

check_PROGRAMS = regexprops test_splitstring
check_SCRIPTS = check-regexprops
regexprops_SOURCES = regexprops.c regextype.c

TESTS =
if CROSS_COMPILING
# The regexprops program needs to be a native executable, so we
# can't build it with a cross-compiler.
else
TESTS += check-regexprops test_splitstring
endif

libfind_a_SOURCES = findutils-version.c
EXTRA_DIST = extendbuf.h buildcmd.h \
	findutils-version.h \
	fdleak.h unused-result.h check-regexprops.sh
SUFFIXES =
MOSTLYCLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =


INCLUDES = -I../gl/lib -I$(top_srcdir)/gl/lib
LDADD = ../gl/lib/libgnulib.a $(LIBINTL)

libfind_a_SOURCES += printquoted.h listfile.h \
	regextype.h dircallback.h safe-atoi.h splitstring.h
libfind_a_SOURCES += listfile.c extendbuf.c buildcmd.c \
	forcefindlib.c qmark.c printquoted.c regextype.c dircallback.c fdleak.c \
	safe-atoi.c splitstring.c

CLEANFILES = check-regexprops \
	$(regexprops_SOURCES:.c=.gcno) \
	$(regexprops_SOURCES:.c=.gcda) \
	$(libfind_a_SOURCES:.c=.gcno) \
	$(libfind_a_SOURCES:.c=.gcda) \
	$(test_splitstring_SOURCES:.c=.gcno) \
	$(test_splitstring_SOURCES:.c=.gcda)

EXTRA_DIST += waitpid.c forcefindlib.c
TESTS_ENVIRONMENT = REGEXPROPS=regexprops$(EXEEXT)
libfind_a_LIBADD = $(FINDLIBOBJS)
libfind_a_DEPENDENCIES = $(FINDLIBOBJS)

check-regexprops: check-regexprops.sh
	cp  $(srcdir)/check-regexprops.sh check-regexprops
	chmod +x check-regexprops

test_splitstring_SOURCES = test_splitstring.c splitstring.c
