# The default database to build and search.
LOCATE_DB = $(localstatedir)/locatedb
localedir = $(datadir)/locale

bin_PROGRAMS = locate
libexec_PROGRAMS = frcode code bigram
bin_SCRIPTS = updatedb
man_MANS = locate.1 updatedb.1 locatedb.5

EXTRA_DIST = locatedb.h updatedb.sh $(man_MANS)
CLEANFILES = updatedb

INCLUDES = -I$(top_srcdir)/lib -I../intl -DLOCATE_DB=\"$(LOCATE_DB)\" -DLOCALEDIR=\"$(localedir)\"

LDADD = ../find/version.o ../lib/libfind.a @INTLLIBS@

$(PROGRAMS) $(LIBPROGRAMS): ../find/version.o ../lib/libfind.a

install:
	$(top_srcdir)/mkinstalldirs $(localstatedir)
