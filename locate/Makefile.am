# The default database to build and search.
AUTOMAKE_OPTIONS = std-options
AM_CFLAGS = $(WARN_CFLAGS)
LOCATE_DB = $(localstatedir)/locatedb
localedir = $(datadir)/locale

AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = \
	frcode$(EXEEXT) \
	code$(EXEEXT) \
	bigram$(EXEEXT)
bin_PROGRAMS = locate
libexec_PROGRAMS = frcode code bigram
bin_SCRIPTS = updatedb
man_MANS = locate.1 updatedb.1 locatedb.5
BUILT_SOURCES = dblocation.texi
EXTRA_DIST = dblocation.texi locatedb.h updatedb.sh $(man_MANS)
CLEANFILES = \
	updatedb \
	$(bigram_SOURCES:.c=.gcno) \
	$(bigram_SOURCES:.c=.gcda) \
	$(code_SOURCES:.c=.gcno) \
	$(code_SOURCES:.c=.gcda) \
	$(frcode_SOURCES:.c=.gcno) \
	$(frcode_SOURCES:.c=.gcda) \
	$(locate_SOURCES:.c=.gcno) \
	$(locate_SOURCES:.c=.gcda)

DISTCLEANFILES = dblocation.texi
locate_SOURCES = locate.c word_io.c
code_SOURCES = code.c word_io.c
locate_TEXINFOS = dblocation.texi

INCLUDES = -I$(top_srcdir)/lib -I../gl/lib -I$(top_srcdir)/gl/lib -I../intl -DLOCATE_DB=\"$(LOCATE_DB)\" -DLOCALEDIR=\"$(localedir)\"

LDADD = ../lib/libfind.a ../gl/lib/libgnulib.a $(LIB_CLOSE) $(LIBINTL)

$(PROGRAMS) $(LIBPROGRAMS): ../lib/libfind.a ../gl/lib/libgnulib.a

# We generate updatedb from the Makefile rather than with AC_OUTPUT in
# configure.ac in order to allow the user better control of what goes
# into it by setting Makefile variables.
updatedb: updatedb.sh Makefile
	rm -f $@
	find=`echo find|sed '$(transform)'`; \
	frcode=`echo frcode|sed '$(transform)'`; \
	bigram=`echo bigram|sed '$(transform)'`; \
	code=`echo code|sed '$(transform)'`; \
	sed \
	-e "s,@""bindir""@,$(bindir)," \
	-e "s,@""libexecdir""@,$(libexecdir)," \
	-e "s,@""LOCATE_DB""@,$(LOCATE_DB)," \
	-e "s,@""VERSION""@,$(VERSION)," \
	-e "s,@""PACKAGE_NAME""@,$(PACKAGE_NAME)," \
	-e "s,@""find""@,$${find}," \
	-e "s,@""frcode""@,$${frcode}," \
	-e "s,@""bigram""@,$${bigram}," \
	-e "s,@""code""@,$${code}," \
	-e "s,@""SORT""@,$(SORT)," \
	-e "s,@""SORT_SUPPORTS_Z""@,$(SORT_SUPPORTS_Z)," \
	$(srcdir)/updatedb.sh > $@
	chmod +x $@

install-data-hook:
	$(top_srcdir)/build-aux/mkinstalldirs $(DESTDIR)$(localstatedir)

dblocation.texi:
	echo '@set LOCATE_DB $(LOCATE_DB)' > $@.tmp
	if test -f $@ && cmp $@.tmp $@ >/dev/null ; then \
	    rm $@.tmp ; \
	else \
	    mv $@.tmp $@ ; \
	fi
.PHONY: dblocation.texi

SUBDIRS = . testsuite

dist-hook: findutils-check-manpages

findutils-check-manpages:
	$(top_srcdir)/build-aux/man-lint.sh $(srcdir) $(man_MANS)
