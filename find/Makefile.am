bin_PROGRAMS = find
find_SOURCES = find.c fstype.c parser.c pred.c tree.c util.c version.c
EXTRA_DIST = defs.h $(man_MANS)
INCLUDES = -I.. -I$(top_srcdir)/lib
LDADD = ../lib/libfind.a
man_MANS = find.1

$(PROGRAMS): ../lib/libfind.a
