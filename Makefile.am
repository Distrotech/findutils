AUTOMAKE_OPTIONS=gnits readme-alpha

EXTRA_DIST = COPYING ChangeLog TODO install-sh config.h.in stamp-h.in \
		THANKS config.rpath
SUBDIRS = gnulib lib find xargs locate doc po m4

ACLOCAL_AMFLAGS = -I gnulib/m4 -I m4

# DISTCLEANFILES = intl/libintl.h

jy_hack_DIST = gnulib/lib/regex.c

dist-hook:
	for file in $(jy_hack_DIST); do \
		cp $(srcdir)/$$file $(distdir)/$$file; \
	done
