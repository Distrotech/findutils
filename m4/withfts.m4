dnl This macro is being phased out; --with-fts is now mandatory.  The
dnl oldfind binary is no longer installed.
AC_DEFUN([FIND_WITH_FTS],
[AC_ARG_WITH([fts],
[  --without-fts           Use an older mechanism for searching the filesystem, instead of using fts()],[with_fts=$withval],[])
  case $with_fts in
        yes) ;;
        '')     with_fts=yes ;;
	 no)	AC_MSG_ERROR([Using --without-fts is not longer supported]) ;;
        *) AC_MSG_ERROR([Invalid value for --with-fts: $with_fts])
  esac
  AM_CONDITIONAL(WITH_FTS, [[test x"${with_fts-no}" != xno]])
  AC_DEFINE([WITH_FTS], 1, [Define if you want to use fts() to do the filesystem search.])
])
