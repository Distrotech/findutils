/* Entries for config.h.in that aren't automatically generated.  */

/* Define if you have the Andrew File System.  */
#undef AFS

/* Define If you want find -nouser and -nogroup to make tables of
   used UIDs and GIDs at startup instead of using getpwuid or
   getgrgid when needed.  Speeds up -nouser and -nogroup unless you
   are running NIS or Hesiod, which make password and group calls
   very expensive.  */
#undef CACHE_IDS

/* Define to use SVR4 statvfs to get filesystem type.  */
#undef FSTYPE_STATVFS

/* Define to use SVR3.2 statfs to get filesystem type.  */
#undef FSTYPE_USG_STATFS

/* Define to use AIX3 statfs to get filesystem type.  */
#undef FSTYPE_AIX_STATFS

/* Define to use 4.3BSD getmntent to get filesystem type.  */
#undef FSTYPE_MNTENT

/* Define to use 4.4BSD and OSF1 statfs to get filesystem type.  */
#undef FSTYPE_STATFS

/* Define to use Ultrix getmnt to get filesystem type.  */
#undef FSTYPE_GETMNT

/* Define to `unsigned long' if <sys/types.h> doesn't define.  */
#undef dev_t

/* Define to `unsigned long' if <sys/types.h> doesn't define.  */
#undef ino_t

/* Define to rpl_fnmatch if the replacement function should be used.  */
#undef fnmatch

/* Define to 1 if assertions should be disabled.  */
#undef NDEBUG

/* Define to rpl_malloc if the replacement function should be used.  */
#undef malloc

/* Define to rpl_memcmp if the replacement function should be used.  */
#undef memcmp

/* Define to gnu_mktime if the replacement function should be used.  */
#undef mktime

/* Define to rpl_realloc if the replacement function should be used.  */
#undef realloc

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define to `unsigned long' or `unsigned long long'
   if <inttypes.h> doesn't define.  */
#undef uintmax_t

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define as 1 if you have the stpcpy function.  */
#undef HAVE_STPCPY
