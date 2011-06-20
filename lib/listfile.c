/* listfile.c -- display a long listing of a file
   Copyright (C) 1991, 1993, 2000, 2004, 2005, 2007, 2008, 2010, 2011
   Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/* config.h must be included first. */
#include <config.h>

/* system headers. */
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <locale.h>
#include <openat.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h> /* for readlink() */

/* gnulib headers. */
#include "areadlink.h"
#include "error.h"
#include "filemode.h"
#include "human.h"
#include "idcache.h"
#include "pathmax.h"
#include "stat-size.h"

/* find headers. */
#include "listfile.h"

/* Since major is a function on SVR4, we can't use `ifndef major'.  */
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#define HAVE_MAJOR
#endif
#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#define HAVE_MAJOR
#endif

#ifdef major			/* Might be defined in sys/types.h.  */
#define HAVE_MAJOR
#endif
#ifndef HAVE_MAJOR
#define major(dev)  (((dev) >> 8) & 0xff)
#define minor(dev)  ((dev) & 0xff)
#endif
#undef HAVE_MAJOR


static void print_name (register const char *p, FILE *stream, int literal_control_chars);


/* NAME is the name to print.
   RELNAME is the path to access it from the current directory.
   STATP is the results of stat or lstat on it.
   Use CURRENT_TIME to decide whether to print yyyy or hh:mm.
   Use OUTPUT_BLOCK_SIZE to determine how to print file block counts
   and sizes.
   STREAM is the stdio stream to print on.  */

void
list_file (const char *name,
	   int dir_fd,
	   char *relname,
	   const struct stat *statp,
	   time_t current_time,
	   int output_block_size,
	   int literal_control_chars,
	   FILE *stream)
{
  char modebuf[12];
  struct tm const *when_local;
  char const *user_name;
  char const *group_name;
  char hbuf[LONGEST_HUMAN_READABLE + 1];

#if HAVE_ST_DM_MODE
  /* Cray DMF: look at the file's migrated, not real, status */
  strmode (statp->st_dm_mode, modebuf);
#else
  strmode (statp->st_mode, modebuf);
#endif

  fprintf (stream, "%6s ",
	   human_readable ((uintmax_t) statp->st_ino, hbuf,
			   human_ceiling,
			   1u, 1u));

  fprintf (stream, "%4s ",
	   human_readable ((uintmax_t) ST_NBLOCKS (*statp), hbuf,
			   human_ceiling,
			   ST_NBLOCKSIZE, output_block_size));


  /* modebuf includes the space between the mode and the number of links,
     as the POSIX "optional alternate access method flag".  */
  fprintf (stream, "%s%3lu ", modebuf, (unsigned long) statp->st_nlink);

  user_name = getuser (statp->st_uid);
  if (user_name)
    fprintf (stream, "%-8s ", user_name);
  else
    fprintf (stream, "%-8lu ", (unsigned long) statp->st_uid);

  group_name = getgroup (statp->st_gid);
  if (group_name)
    fprintf (stream, "%-8s ", group_name);
  else
    fprintf (stream, "%-8lu ", (unsigned long) statp->st_gid);

  if (S_ISCHR (statp->st_mode) || S_ISBLK (statp->st_mode))
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    fprintf (stream, "%3lu, %3lu ",
	     (unsigned long) major (statp->st_rdev),
	     (unsigned long) minor (statp->st_rdev));
#else
    fprintf (stream, "         ");
#endif
  else
    fprintf (stream, "%8s ",
	     human_readable ((uintmax_t) statp->st_size, hbuf,
			     human_ceiling,
			     1,
			     output_block_size < 0 ? output_block_size : 1));

  if ((when_local = localtime (&statp->st_mtime)))
    {
      char init_bigbuf[256];
      char *buf = init_bigbuf;
      size_t bufsize = sizeof init_bigbuf;

      /* Use strftime rather than ctime, because the former can produce
	 locale-dependent names for the month (%b).

	 Output the year if the file is fairly old or in the future.
	 POSIX says the cutoff is 6 months old;
	 approximate this by 6*30 days.
	 Allow a 1 hour slop factor for what is considered "the future",
	 to allow for NFS server/client clock disagreement.  */
      char const *fmt =
	((current_time - 6 * 30 * 24 * 60 * 60 <= statp->st_mtime
	  && statp->st_mtime <= current_time + 60 * 60)
	 ? "%b %e %H:%M"
	 : "%b %e  %Y");

      while (!strftime (buf, bufsize, fmt, when_local))
	buf = alloca (bufsize *= 2);

      fprintf (stream, "%s ", buf);
    }
  else
    {
      /* The time cannot be represented as a local time;
	 print it as a huge integer number of seconds.  */
      int width = 12;

      if (statp->st_mtime < 0)
	{
	  char const *num = human_readable (- (uintmax_t) statp->st_mtime,
					    hbuf, human_ceiling, 1, 1);
	  int sign_width = width - strlen (num);
	  fprintf (stream, "%*s%s ",
		   sign_width < 0 ? 0 : sign_width, "-", num);
	}
      else
	fprintf (stream, "%*s ", width,
		 human_readable ((uintmax_t) statp->st_mtime, hbuf,
				 human_ceiling,
				 1, 1));
    }

  print_name (name, stream, literal_control_chars);

  if (S_ISLNK (statp->st_mode))
    {
      char *linkname = areadlinkat (dir_fd, relname);
      if (linkname)
	{
	  fputs (" -> ", stream);
	  print_name (linkname, stream, literal_control_chars);
	}
      else
	{
	  /* POSIX requires in the case of find that if we issue a
	   * diagnostic we should have a nonzero status.  However,
	   * this function doesn't have a way of telling the caller to
	   * do that.  However, since this function is only used when
	   * processing "-ls", we're already using an extension.
	   */
	  error (0, errno, "%s", name);
	}
      free (linkname);
    }
  putc ('\n', stream);
}


static void
print_name_without_quoting (const char *p, FILE *stream)
{
  fprintf (stream, "%s", p);
}


static void
print_name_with_quoting (register const char *p, FILE *stream)
{
  register unsigned char c;

  while ((c = *p++) != '\0')
    {
      switch (c)
	{
	case '\\':
	  fprintf (stream, "\\\\");
	  break;

	case '\n':
	  fprintf (stream, "\\n");
	  break;

	case '\b':
	  fprintf (stream, "\\b");
	  break;

	case '\r':
	  fprintf (stream, "\\r");
	  break;

	case '\t':
	  fprintf (stream, "\\t");
	  break;

	case '\f':
	  fprintf (stream, "\\f");
	  break;

	case ' ':
	  fprintf (stream, "\\ ");
	  break;

	case '"':
	  fprintf (stream, "\\\"");
	  break;

	default:
	  if (c > 040 && c < 0177)
	    putc (c, stream);
	  else
	    fprintf (stream, "\\%03o", (unsigned int) c);
	}
    }
}

static void print_name (register const char *p, FILE *stream, int literal_control_chars)
{
  if (literal_control_chars)
    print_name_without_quoting (p, stream);
  else
    print_name_with_quoting (p, stream);
}
