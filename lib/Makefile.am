noinst_LIBRARIES = libfind.a
libfind_a_SOURCES = regex.c \
			regex.h \
			dirname.c \
			error.c \
			filemode.c \
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
			xmalloc.c \
			getline.c \
			xstrdup.c \
			xgetcwd.c \
			fnmatch.c \
			fnmatch.h

libfind_a_LIBADD = @LIBOBJS@ \
		@ALLOCA@


# Are these really needed?
EXTRA_DIST = modetype.h \
		wait.h \
		alloca.c

INCLUDES = -I.. -I$(srcdir)
