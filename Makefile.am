AUTOMAKE_OPTIONS=gnits readme-alpha

EXTRA_DIST = COPYING ChangeLog TODO install-sh config.h.in stamp-h.in \
		THANKS config.rpath
SUBDIRS = gnulib lib find xargs locate doc po m4

ACLOCAL_AMFLAGS = -I gnulib/m4 -I m4

CONFIG_CLEAN_FILES = gnulib/lib/regex.c


# DISTCLEANFILES = intl/libintl.h

##
## regex.c seems to get left out if I use automake-1.9 but not 
## if I use automake-1.7.   Hence dist-hook has to be able to 
## copy regex.c into the relevant (read-only) directory if it 
## is not already there, but needs to avoid doing so if the 
## file is already in place.   Ugh.  
##                             -- James Youngman <jay@gnu.org>
## 
dist-hook:
	if test -f $(distdir)/gnulib/lib/regex.c ; then \
		echo regex.c is already in place.  Great. ; \
	else \
		echo Making $(distdir)/gnulib/lib writable ... ; \
		chmod +w $(distdir)/gnulib/lib ; \
		echo Copying $(srcdir)/gnulib/lib/regex.c to $(distdir)/gnulib/lib ; \
		cp $(srcdir)/gnulib/lib/regex.c $(distdir)/gnulib/lib/regex.c ; \
	fi
#	ls -ld $(srcdir)/gnulib/lib/regex.c $(distdir)/gnulib/lib
