AUTOMAKE_OPTIONS = ansi2knr
noinst_LIBRARIES = libfind.a
libfind_a_SOURCES = regex.h \
			dirname.c \
			error.c \
			error.h \
			filemode.c \
			filemode.h \
			getopt.c \
			getopt.h \
			getopt1.c \
			idcache.c \
			listfile.c \
			pathmax.h \
			modechange.c \
			modechange.h \
			nextelem.c \
			savedir.c \
			savedir.h \
			xmalloc.c \
			xalloc.h \
			xstrdup.c \
			xgetcwd.c \
			fnmatch.h

libfind_a_LIBADD = @LIBOBJS@ \
		@ALLOCA@


# Are these really needed?
EXTRA_DIST = modetype.h \
		wait.h \
		alloca.c \
		lstat.c

INCLUDES = -I.. -I$(srcdir)
