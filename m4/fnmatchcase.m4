#serial 2

dnl Determine whether to add fnmatch.o to LIBOBJS and to
dnl define fnmatch to rpl_fnmatch.
dnl

# AC_FUNC_FNMATCH
# ---------------
# We look for fnmatch.h to avoid that the test fails in C++.
AC_DEFUN(kd_FUNC_FNMATCH_CASE,
[AC_CHECK_HEADERS(fnmatch.h)
AC_CACHE_CHECK(for working fnmatch with case folding, ac_cv_func_fnmatch_works,
# Some versions of Solaris or SCO have a broken fnmatch function.
# So we run a test program.  If we are cross-compiling, take no chance.
# Also want FNM_CASEFOLD
# Thanks to John Oleynick and Franc,ois Pinard for this test.
[AC_TRY_RUN(
[#include <stdio.h>
#define _GNU_SOURCE
#if HAVE_FNMATCH_H
# include <fnmatch.h>
#endif

int
main ()
{
  exit (fnmatch ("A*", "abc", FNM_CASEFOLD) != 0);
}],
ac_cv_func_fnmatch_works=yes, ac_cv_func_fnmatch_works=no,
ac_cv_func_fnmatch_works=no)])
if test $ac_cv_func_fnmatch_works = yes; then
  AC_DEFINE(HAVE_FNMATCH, 1,
            [Define if your system has a working `fnmatch' function with case folding.])
fi
])# AC_FUNC_FNMATCH


AC_DEFUN(kd_FUNC_FNMATCH_CASE_RPL,
[
  AC_REQUIRE([AM_GLIBC])
  kd_FUNC_FNMATCH_CASE
  if test $ac_cv_func_fnmatch_works = no \
      && test $ac_cv_gnu_library = no; then
    AC_SUBST(LIBOBJS)
    LIBOBJS="$LIBOBJS fnmatch.$ac_objext"
    AC_DEFINE_UNQUOTED(fnmatch, rpl_fnmatch,
      [Define to rpl_fnmatch if the replacement function should be used.])
  fi
])
