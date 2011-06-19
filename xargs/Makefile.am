AUTOMAKE_OPTIONS = std-options
AM_CFLAGS = $(WARN_CFLAGS)
localedir = $(datadir)/locale
bin_PROGRAMS = xargs
man_MANS = xargs.1
INCLUDES = -I.. -I../gl/lib -I$(top_srcdir)/gl/lib -I$(top_srcdir)/lib -I../intl -DLOCALEDIR=\"$(localedir)\"
LDADD = ../lib/libfind.a ../gl/lib/libgnulib.a $(LIB_CLOSE) $(LIBINTL)
EXTRA_DIST = $(man_MANS)
SUBDIRS = . testsuite

CLEANFILES = $(xargs_SOURCES:.c=.gcno) $(xargs_SOURCES:.c=.gcda)

$(PROGRAMS): ../lib/libfind.a ../gl/lib/libgnulib.a

dist-hook: findutils-check-manpages

findutils-check-manpages:
	$(top_srcdir)/build-aux/man-lint.sh $(srcdir) $(man_MANS)
