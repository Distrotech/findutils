AC_DEFUN([FIND_WITH_FTS],
[AC_ARG_WITH(fts,
[  --with-fts         	  Use fts() to search the filesystem],[])
AC_DEFINE(WITH_FTS, 1, [Define if you want to use fts() to do the filesystem search.])
AM_CONDITIONAL(WITH_FTS, [[test x"${with_fts-no}" != xno]])
])
