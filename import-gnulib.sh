#! /bin/sh

##
## This script is intended to populate the "gnulib" directory 
## with a subset of the gnulib code, as provided by "gnulib-tool".
##
## To use it, run this script, speficying the location of the 
## gnulib code as the only argument.   Some sanity-checking is done 
## before we commit to modifying things.   The gnulib code is placed
## in the "gnulib" subdirectory, which is where the buid files expect 
## it to be. 
## 

destdir="gnulib"


modules="\
alloca  argmatch  dirname error fileblocks  fnmatch  \
getline getopt human idcache  lstat malloc memcmp memset mktime	  \
modechange   pathmax quotearg realloc regex rpmatch savedir stat  \
stpcpy strdup strftime  strstr strtol strtoul strtoull strtoumax  \
xalloc xgetcwd  xstrtol  xstrtoumax yesno human basename filemode \
getline"

if test $# -lt 1
then
    echo "You need to specify the name of the directory containing gnulib" >&2
    exit 1
fi

if test -d "$1"
then
    true
else
    echo "$1 is not a directory" >&2
    exit 1
fi

if test -e "$1"/gnulib-tool
then
    true
else
    echo "$1/gnulib-tool does not exist, did you specify the right directory?" >&2
    exit 1
fi

if test -x "$1"/gnulib-tool
then
    true
else
    echo "$1/gnulib-tool is not executable" >&2
    exit 1
fi

if test -x "$1"/gnulib-tool
then
    exec "$1"/gnulib-tool --create-testdir --dir="$destdir" --lib=libgnulib $modules
fi

