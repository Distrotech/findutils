bin_PROGRAMS = xargs
man_MANS = xargs.1
INCLUDES = -I.. -I$(top_srcdir)/lib
LDADD = ../find/version.o ../lib/libfind.a
EXTRA_DIST = $(man_MANS)

$(PROGRAMS): ../find/version.o ../lib/libfind.a

