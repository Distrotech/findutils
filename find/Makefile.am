AUTOMAKE_OPTIONS = std-options
localedir = $(datadir)/locale
bin_PROGRAMS = find
# noinst_PROGRAMS = regexprops
# regexprops_SOURCES = regexprops.c
find_SOURCES = find.c fstype.c parser.c pred.c tree.c util.c version.c
EXTRA_DIST = defs.h $(man_MANS)
INCLUDES = -I../gnulib/lib -I$(top_srcdir)/lib -I$(top_srcdir)/gnulib/lib -I../intl -DLOCALEDIR=\"$(localedir)\"
LDADD = ../lib/libfind.a ../gnulib/lib/libgnulib.a @INTLLIBS@
man_MANS = find.1
SUBDIRS = . testsuite

#$(PROGRAMS): ../lib/libfind.a

