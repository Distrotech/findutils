/* savedirinfo.c -- Save the list of files in a directory, with additional information.

   Copyright 1990, 1997, 1998, 1999, 2000, 2001, 2003, 2004, 2005 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by James Youngman, <jay@gnu.org>. */
/* Derived from savedir.c, written by David MacKenzie <djm@gnu.org>. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <errno.h>

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef CLOSEDIR_VOID
/* Fake a return value. */
# define CLOSEDIR(d) (closedir (d), 0)
#else
# define CLOSEDIR(d) closedir (d)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"
#include "extendbuf.h"
#include "savedirinfo.h"

/* In order to use struct dirent.d_type, it has to be enabled on the
 * configure command line, and we have to have a d_type member in 
 * 'struct dirent'.
 */
#if !defined(USE_STRUCT_DIRENT_D_TYPE)
/* Not enabled, hence pretend it is absent. */
#undef HAVE_STRUCT_DIRENT_D_TYPE
#endif
#if !defined(HAVE_STRUCT_DIRENT_D_TYPE)
/* Not present, so cannot use it. */
#undef USE_STRUCT_DIRENT_D_TYPE
#endif


#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && defined(USE_STRUCT_DIRENT_D_TYPE)
/* Convert the value of struct dirent.d_type into a value for 
 * struct stat.st_mode (at least the file type bits), or zero
 * if the type is DT_UNKNOWN or is a value we don't know about.
 */
static mode_t
type_to_mode(unsigned type)
{
  mode_t result = 0;
  
  switch (type)
    {
#ifdef DT_FIFO
    case DT_FIFO: return S_IFIFO;
#endif
#ifdef DT_CHR
    case DT_CHR:  return S_IFCHR;
#endif		  
#ifdef DT_DIR	  
    case DT_DIR:  return S_IFDIR;
#endif		  
#ifdef DT_BLK	  
    case DT_BLK:  return S_IFBLK;
#endif		  
#ifdef DT_REG	  
    case DT_REG:  return S_IFREG;
#endif		  
#ifdef DT_LNK	  
    case DT_LNK:  return S_IFLNK;
#endif
#ifdef DT_SOCK
    case DT_SOCK: return S_IFSOCK;
#endif
    default:
      return 0;			/* Unknown. */
    }
}

#endif


/* Return a freshly allocated string containing the filenames
   in directory DIR, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Return NULL (setting errno) if DIR cannot be opened, read, or closed.  */

char *
savedirinfo (const char *dir, struct savedir_dirinfo **extra)
{
  DIR *dirp;
  struct dirent *dp;
  char *name_space;
  size_t namebuf_allocated = 0u, namebuf_used = 0u;
#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && defined(USE_STRUCT_DIRENT_D_TYPE)
  size_t extra_allocated = 0u, extra_used = 0u;
  struct savedir_dirinfo *info = NULL;
#endif
  int save_errno;

  if (extra)
    *extra = NULL;
  
  dirp = opendir (dir);
  if (dirp == NULL)
    return NULL;

  errno = 0;
  name_space = NULL;
  while ((dp = readdir (dirp)) != NULL)
    {
      /* Skip "", ".", and "..".  "" is returned by at least one buggy
         implementation: Solaris 2.4 readdir on NFS file systems.  */
      char const *entry = dp->d_name;
      if (entry[entry[0] != '.' ? 0 : entry[1] != '.' ? 1 : 2] != '\0')
	{
	  /* Remember the name. */
	  size_t entry_size = strlen (entry) + 1;
	  name_space = extendbuf(name_space, namebuf_used+entry_size, &namebuf_allocated);
	  memcpy (name_space + namebuf_used, entry, entry_size);
	  namebuf_used += entry_size;


#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && defined(USE_STRUCT_DIRENT_D_TYPE)
	  /* Remember the type. */
	  if (extra)
	    {
	      info = extendbuf(info,
			       (extra_used+1) * sizeof(struct savedir_dirinfo),
			       &extra_allocated);
	      info[extra_used].type_info = type_to_mode(dp->d_type);
	      ++extra_used;
	    }
#endif
	}
    }
  
  name_space = extendbuf(name_space, namebuf_used+1, &namebuf_allocated);
  name_space[namebuf_used] = '\0';
  
  save_errno = errno;
  if (CLOSEDIR (dirp) != 0)
    save_errno = errno;
  if (save_errno != 0)
    {
      free (name_space);
      errno = save_errno;
      return NULL;
    }
  
#if defined(HAVE_STRUCT_DIRENT_D_TYPE) && defined(USE_STRUCT_DIRENT_D_TYPE)
  if (extra && info)
    *extra = info;
#endif
  
  return name_space;
}
