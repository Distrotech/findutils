AUTOMAKE_OPTIONS = ansi2knr
noinst_LIBRARIES = libfind.a
localedir = $(datadir)/locale
INCLUDES = -I../intl -DLOCALEDIR=\"$(localedir)\"
libfind_a_SOURCES = regex.h \
			dirname.c \
			dirname.h \
			error.c \
			error.h \
			filemode.c \
			filemode.h \
			getopt.c \
			getopt.h \
			getopt1.c \
			idcache.c \
			listfile.c \
			modechange.c \
			modechange.h \
			pathmax.h \
			nextelem.c \
			savedir.c \
			savedir.h \
			xmalloc.c \
			xalloc.h \
			xstrdup.c \
			xgetcwd.c \
			fnmatch.h \
			xstrtol.c \
			xstrtol.h \
			xstrtoul.c \
			xstrtoul.h \
			getline.c \
			getline.h \
			basename.c

libfind_a_LIBADD = @LIBOBJS@ \
		@ALLOCA@


# Are these really needed?
EXTRA_DIST = modetype.h \
		wait.h \
		alloca.c \
		getline.h \
		xstat.in

BUILT_SOURCES = lstat.c stat.c
DISTCLEANFILES = lstat.c stat.c

lstat.c: xstat.in
	sed \
	  -e '/@IGNORE@/d' \
	  -e 's/@xstat@/lstat/g' \
	  -e '/_LSTAT_ONLY@/d' \
	  -e '/@BEGIN_STAT_ONLY@/,/@END_STAT_ONLY@/d' \
	  $(srcdir)/xstat.in > $@-t
	mv $@-t $@

stat.c: xstat.in
	sed \
	  -e '/@IGNORE@/d' \
	  -e 's/@xstat@/stat/g' \
	  -e '/_STAT_ONLY@/d' \
	  -e '/@BEGIN_LSTAT_ONLY@/,/@END_LSTAT_ONLY@/d' \
	  $(srcdir)/xstat.in > $@-t
	mv $@-t $@
