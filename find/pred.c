/* pred.c -- execute the expression tree.
   Copyright (C) 1990, 91, 92, 93, 94, 2000, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.
*/

#include "defs.h"

#include <fnmatch.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include "../gnulib/lib/xalloc.h"
#include "../gnulib/lib/dirname.h"
#include "../gnulib/lib/human.h"
#include "modetype.h"
#include "filemode.h"
#include "wait.h"
#include "buildcmd.h"

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif
#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
/* See locate.c for explanation as to why not use (String) */
# define N_(String) String
#endif

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
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
#define CLOSEDIR(d) (closedir (d), 0)
#else
#define CLOSEDIR(d) closedir (d)
#endif

extern int yesno ();


/* Get or fake the disk device blocksize.
   Usually defined by sys/param.h (if at all).  */
#ifndef DEV_BSIZE
# ifdef BSIZE
#  define DEV_BSIZE BSIZE
# else /* !BSIZE */
#  define DEV_BSIZE 4096
# endif /* !BSIZE */
#endif /* !DEV_BSIZE */

/* Extract or fake data from a `struct stat'.
   ST_BLKSIZE: Preferred I/O blocksize for the file, in bytes.
   ST_NBLOCKS: Number of blocks in the file, including indirect blocks.
   ST_NBLOCKSIZE: Size of blocks used when calculating ST_NBLOCKS.  */
#ifndef HAVE_STRUCT_STAT_ST_BLOCKS
# define ST_BLKSIZE(statbuf) DEV_BSIZE
# if defined(_POSIX_SOURCE) || !defined(BSIZE) /* fileblocks.c uses BSIZE.  */
#  define ST_NBLOCKS(statbuf) \
  (S_ISREG ((statbuf).st_mode) \
   || S_ISDIR ((statbuf).st_mode) \
   ? (statbuf).st_size / ST_NBLOCKSIZE + ((statbuf).st_size % ST_NBLOCKSIZE != 0) : 0)
# else /* !_POSIX_SOURCE && BSIZE */
#  define ST_NBLOCKS(statbuf) \
  (S_ISREG ((statbuf).st_mode) \
   || S_ISDIR ((statbuf).st_mode) \
   ? st_blocks ((statbuf).st_size) : 0)
# endif /* !_POSIX_SOURCE && BSIZE */
#else /* HAVE_STRUCT_STAT_ST_BLOCKS */
/* Some systems, like Sequents, return st_blksize of 0 on pipes. */
# define ST_BLKSIZE(statbuf) ((statbuf).st_blksize > 0 \
			       ? (statbuf).st_blksize : DEV_BSIZE)
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
/* HP-UX counts st_blocks in 1024-byte units.
   This loses when mixing HP-UX and BSD filesystems with NFS.  */
#  define ST_NBLOCKSIZE 1024
# else /* !hpux */
#  if defined(_AIX) && defined(_I386)
/* AIX PS/2 counts st_blocks in 4K units.  */
#   define ST_NBLOCKSIZE (4 * 1024)
#  else /* not AIX PS/2 */
#   if defined(_CRAY)
#    define ST_NBLOCKS(statbuf) \
  (S_ISREG ((statbuf).st_mode) \
   || S_ISDIR ((statbuf).st_mode) \
   ? (statbuf).st_blocks * ST_BLKSIZE(statbuf)/ST_NBLOCKSIZE : 0)
#   endif /* _CRAY */
#  endif /* not AIX PS/2 */
# endif /* !hpux */
#endif /* HAVE_STRUCT_STAT_ST_BLOCKS */

#ifndef ST_NBLOCKS
# define ST_NBLOCKS(statbuf) \
  (S_ISREG ((statbuf).st_mode) \
   || S_ISDIR ((statbuf).st_mode) \
   ? (statbuf).st_blocks : 0)
#endif

#ifndef ST_NBLOCKSIZE
# define ST_NBLOCKSIZE 512
#endif

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static boolean insert_lname PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr, boolean ignore_case));

static char *format_date PARAMS((time_t when, int kind));
static char *ctime_format PARAMS((time_t when));

#ifdef	DEBUG
struct pred_assoc
{
  PFB pred_func;
  char *pred_name;
};

struct pred_assoc pred_table[] =
{
  {pred_amin, "amin    "},
  {pred_and, "and     "},
  {pred_anewer, "anewer  "},
  {pred_atime, "atime   "},
  {pred_close, ")       "},
  {pred_amin, "cmin    "},
  {pred_cnewer, "cnewer  "},
  {pred_comma, ",       "},
  {pred_ctime, "ctime   "},
  {pred_delete, "delete  "},
  {pred_empty, "empty   "},
  {pred_exec, "exec    "},
  {pred_execdir, "execdir "},
  {pred_false, "false   "},
  {pred_fprint, "fprint  "},
  {pred_fprint0, "fprint0 "},
  {pred_fprintf, "fprintf "},
  {pred_fstype, "fstype  "},
  {pred_gid, "gid     "},
  {pred_group, "group   "},
  {pred_ilname, "ilname  "},
  {pred_iname, "iname   "},
  {pred_inum, "inum    "},
  {pred_ipath, "ipath   "},
  {pred_links, "links   "},
  {pred_lname, "lname   "},
  {pred_ls, "ls      "},
  {pred_amin, "mmin    "},
  {pred_mtime, "mtime   "},
  {pred_name, "name    "},
  {pred_negate, "not     "},
  {pred_newer, "newer   "},
  {pred_nogroup, "nogroup "},
  {pred_nouser, "nouser  "},
  {pred_ok, "ok      "},
  {pred_okdir, "okdir   "},
  {pred_open, "(       "},
  {pred_or, "or      "},
  {pred_path, "path    "},
  {pred_perm, "perm    "},
  {pred_print, "print   "},
  {pred_print0, "print0  "},
  {pred_prune, "prune   "},
  {pred_regex, "regex   "},
  {pred_samefile,"samefile "},
  {pred_size, "size    "},
  {pred_true, "true    "},
  {pred_type, "type    "},
  {pred_uid, "uid     "},
  {pred_used, "used    "},
  {pred_user, "user    "},
  {pred_xtype, "xtype   "},
  {0, "none    "}
};

struct op_assoc
{
  short type;
  char *type_name;
};

struct op_assoc type_table[] =
{
  {NO_TYPE, "no          "},
  {PRIMARY_TYPE, "primary      "},
  {UNI_OP, "uni_op      "},
  {BI_OP, "bi_op       "},
  {OPEN_PAREN, "open_paren  "},
  {CLOSE_PAREN, "close_paren "},
  {-1, "unknown     "}
};

struct prec_assoc
{
  short prec;
  char *prec_name;
};

struct prec_assoc prec_table[] =
{
  {NO_PREC, "no      "},
  {COMMA_PREC, "comma   "},
  {OR_PREC, "or      "},
  {AND_PREC, "and     "},
  {NEGATE_PREC, "negate  "},
  {MAX_PREC, "max     "},
  {-1, "unknown "}
};
#endif	/* DEBUG */

/* Predicate processing routines.
 
   PATHNAME is the full pathname of the file being checked.
   *STAT_BUF contains information about PATHNAME.
   *PRED_PTR contains information for applying the predicate.
 
   Return true if the file passes this predicate, false if not. */


/* pred_timewindow
 *
 * Returns true if THE_TIME is 
 * COMP_GT: after the specified time
 * COMP_LT: before the specified time
 * COMP_EQ: less than WINDOW seconds after the specified time.
 */
static boolean
pred_timewindow(time_t the_time, struct predicate const *pred_ptr, int window)
{
  switch (pred_ptr->args.info.kind)
    {
    case COMP_GT:
      if (the_time > (time_t) pred_ptr->args.info.l_val)
	return true;
      break;
    case COMP_LT:
      if (the_time < (time_t) pred_ptr->args.info.l_val)
	return true;
      break;
    case COMP_EQ:
      if ((the_time >= (time_t) pred_ptr->args.info.l_val)
	  && (the_time < (time_t) pred_ptr->args.info.l_val + window))
	return true;
      break;
    }
  return false;
}


boolean
pred_amin (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  return pred_timewindow(stat_buf->st_atime, pred_ptr, 60);
}

boolean
pred_and (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  int match = 0;
  
  if (pred_ptr->pred_left == NULL
      || (*pred_ptr->pred_left->pred_func) (pathname, stat_buf,
					    pred_ptr->pred_left))
    {
      /* Check whether we need a stat here. */
      if (get_info(pathname, state.rel_pathname, stat_buf, pred_ptr) != 0)
	    return false;
      return ((*pred_ptr->pred_right->pred_func) (pathname, stat_buf,
						  pred_ptr->pred_right));
    }
  else
    return (false);
}

boolean
pred_anewer (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  
  if (stat_buf->st_atime > pred_ptr->args.time)
    return (true);
  return (false);
}

boolean
pred_atime (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  return pred_timewindow(stat_buf->st_atime, pred_ptr, DAYSECS);
}

boolean
pred_close (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  (void) &stat_buf;
  (void) &pred_ptr;
  
  return true;
}

boolean
pred_cmin (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  return pred_timewindow(stat_buf->st_ctime, pred_ptr, 60);
}

boolean
pred_cnewer (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  if (stat_buf->st_ctime > pred_ptr->args.time)
    return true;
  else
    return false;
}

boolean
pred_comma (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  if (pred_ptr->pred_left != NULL)
    (*pred_ptr->pred_left->pred_func) (pathname, stat_buf,
				       pred_ptr->pred_left);
  /* Check whether we need a stat here. */
  /* TODO: what about need_type? */
  if (get_info(pathname, state.rel_pathname, stat_buf, pred_ptr) != 0)
    return false;
  return ((*pred_ptr->pred_right->pred_func) (pathname, stat_buf,
					      pred_ptr->pred_right));
}

boolean
pred_ctime (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  return pred_timewindow(stat_buf->st_ctime, pred_ptr, DAYSECS);
}

boolean
pred_delete (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pred_ptr;
  (void) stat_buf;
  if (strcmp (state.rel_pathname, "."))
    {
      if (0 != remove (state.rel_pathname))
	{
	  error (0, errno, "cannot delete %s", pathname);
	  return false;
	}
      else
	{
	  return true;
	}
    }
  
  /* nothing to do. */
  return true;
}

boolean
pred_empty (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  (void) pred_ptr;
  
  if (S_ISDIR (stat_buf->st_mode))
    {
      DIR *d;
      struct dirent *dp;
      boolean empty = true;

      errno = 0;
      d = opendir (state.rel_pathname);
      if (d == NULL)
	{
	  error (0, errno, "%s", pathname);
	  state.exit_status = 1;
	  return false;
	}
      for (dp = readdir (d); dp; dp = readdir (d))
	{
	  if (dp->d_name[0] != '.'
	      || (dp->d_name[1] != '\0'
		  && (dp->d_name[1] != '.' || dp->d_name[2] != '\0')))
	    {
	      empty = false;
	      break;
	    }
	}
      if (CLOSEDIR (d))
	{
	  error (0, errno, "%s", pathname);
	  state.exit_status = 1;
	  return false;
	}
      return (empty);
    }
  else if (S_ISREG (stat_buf->st_mode))
    return (stat_buf->st_size == 0);
  else
    return (false);
}

#if defined(NEW_EXEC)
static boolean
new_impl_pred_exec (const char *pathname, struct stat *stat_buf,
		    struct predicate *pred_ptr,
		    const char *prefix, size_t pfxlen)
{
  struct exec_val *execp = &pred_ptr->args.exec_vec;
  size_t len = strlen(pathname);
  
  if (execp->multiple)
    {
      /* Push the argument onto the current list. 
       * The command may or may not be run at this point, 
       * depending on the command line length limits.
       */
      bc_push_arg(&execp->ctl,
		  &execp->state,
		  pathname, len+1,
		  prefix, pfxlen,
		  0);
      
      /* POSIX: If the primary expression is punctuated by a plus
       * sign, the primary shall always evaluate as true
       */
      return 0;
    }
  else
    {
      int i;

      for (i=0; i<execp->num_args; ++i)
	{
	  bc_do_insert(&execp->ctl,
		       &execp->state,
		       execp->replace_vec[i],
		       strlen(execp->replace_vec[i]),
		       prefix, pfxlen,
		       pathname, len,
		       0);
	}

      /* Actually invoke the command. */
      return  execp->ctl.exec_callback(&execp->ctl,
					&execp->state);
    }
}
#else
static boolean
old_impl_pred_exec (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  int i;
  int path_pos;
  struct exec_val *execp;	/* Pointer for efficiency. */

  (void) pathname;
  (void) stat_buf;
  
  execp = &pred_ptr->args.exec_vec;

  /* Replace "{}" with the real path in each affected arg. */
  for (path_pos = 0; execp->paths[path_pos].offset >= 0; path_pos++)
    {
      register char *from, *to;

      i = execp->paths[path_pos].offset;
      execp->vec[i] =
	xmalloc (strlen (execp->paths[path_pos].origarg) + 1
		 + (strlen (pathname) - 2) * execp->paths[path_pos].count);
      for (from = execp->paths[path_pos].origarg, to = execp->vec[i]; *from; )
	if (from[0] == '{' && from[1] == '}')
	  {
	    to = stpcpy (to, pathname);
	    from += 2;
	  }
	else
	  *to++ = *from++;
      *to = *from;		/* Copy null. */
    }

  i = launch (pred_ptr);

  /* Free the temporary args. */
  for (path_pos = 0; execp->paths[path_pos].offset >= 0; path_pos++)
    free (execp->vec[execp->paths[path_pos].offset]);

  return (i);
}
#endif

boolean
pred_exec (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
#if defined(NEW_EXEC)
  return new_impl_pred_exec(pathname, stat_buf, pred_ptr, NULL, 0);
#else
  return old_impl_pred_exec(pathname, stat_buf, pred_ptr);
#endif
}

boolean
pred_execdir (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  const char *s = basename(pathname);
  return new_impl_pred_exec(s, stat_buf, pred_ptr, "./", 2);
}

boolean
pred_false (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  (void) &stat_buf;
  (void) &pred_ptr;

  
  return (false);
}

boolean
pred_fls (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  list_file (pathname, state.rel_pathname, stat_buf, options.start_time,
	     options.output_block_size, pred_ptr->args.stream);
  return (true);
}

boolean
pred_fprint (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  (void) &stat_buf;
  
  fputs (pathname, pred_ptr->args.stream);
  putc ('\n', pred_ptr->args.stream);
  return (true);
}

boolean
pred_fprint0 (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  (void) &stat_buf;
  
  fputs (pathname, pred_ptr->args.stream);
  putc (0, pred_ptr->args.stream);
  return (true);
}

boolean
pred_fprintf (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  FILE *fp = pred_ptr->args.printf_vec.stream;
  struct segment *segment;
  char *cp;
  char hbuf[LONGEST_HUMAN_READABLE + 1];

  for (segment = pred_ptr->args.printf_vec.segment; segment;
       segment = segment->next)
    {
      if (segment->kind & 0xff00) /* Component of date. */
	{
	  time_t t;

	  switch (segment->kind & 0xff)
	    {
	    case 'A':
	      t = stat_buf->st_atime;
	      break;
	    case 'C':
	      t = stat_buf->st_ctime;
	      break;
	    case 'T':
	      t = stat_buf->st_mtime;
	      break;
	    default:
	      abort ();
	    }
	  fprintf (fp, segment->text,
		   format_date (t, (segment->kind >> 8) & 0xff));
	  continue;
	}

      switch (segment->kind)
	{
	case KIND_PLAIN:	/* Plain text string (no % conversion). */
	  fwrite (segment->text, 1, segment->text_len, fp);
	  break;
	case KIND_STOP:		/* Terminate argument and flush output. */
	  fwrite (segment->text, 1, segment->text_len, fp);
	  fflush (fp);
	  return (true);
	case 'a':		/* atime in `ctime' format. */
	  fprintf (fp, segment->text, ctime_format (stat_buf->st_atime));
	  break;
	case 'b':		/* size in 512-byte blocks */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) ST_NBLOCKS (*stat_buf),
				   hbuf, human_ceiling,
				   ST_NBLOCKSIZE, 512));
	  break;
	case 'c':		/* ctime in `ctime' format */
	  fprintf (fp, segment->text, ctime_format (stat_buf->st_ctime));
	  break;
	case 'd':		/* depth in search tree */
	  fprintf (fp, segment->text, state.curdepth);
	  break;
	case 'D':		/* Device on which file exists (stat.st_dev) */
	  fprintf (fp, segment->text, 
		   human_readable ((uintmax_t) stat_buf->st_dev, hbuf,
				   human_ceiling, 1, 1));
	  break;
	case 'f':		/* basename of path */
	  fprintf (fp, segment->text, base_name (pathname));
	  break;
	case 'F':		/* filesystem type */
	  fprintf (fp, segment->text, filesystem_type (stat_buf));
	  break;
	case 'g':		/* group name */
	  {
	    struct group *g;

	    g = getgrgid (stat_buf->st_gid);
	    if (g)
	      {
		segment->text[segment->text_len] = 's';
		fprintf (fp, segment->text, g->gr_name);
		break;
	      }
	    /* else fallthru */
	  }
	case 'G':		/* GID number */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) stat_buf->st_gid, hbuf,
				   human_ceiling, 1, 1));
	  break;
	case 'h':		/* leading directories part of path */
	  {
	    char cc;

	    cp = strrchr (pathname, '/');
	    if (cp == NULL)	/* No leading directories. */
	      break;
	    cc = *cp;
	    *cp = '\0';
	    fprintf (fp, segment->text, pathname);
	    *cp = cc;
	    break;
	  }
	case 'H':		/* ARGV element file was found under */
	  {
	    char cc = pathname[state.path_length];

	    pathname[state.path_length] = '\0';
	    fprintf (fp, segment->text, pathname);
	    pathname[state.path_length] = cc;
	    break;
	  }
	case 'i':		/* inode number */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) stat_buf->st_ino, hbuf,
				   human_ceiling,
				   1, 1));
	  break;
	case 'k':		/* size in 1K blocks */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) ST_NBLOCKS (*stat_buf),
				   hbuf, human_ceiling,
				   ST_NBLOCKSIZE, 1024)); 
	  break;
	case 'l':		/* object of symlink */
#ifdef S_ISLNK
	  {
	    char *linkname = 0;

	    if (S_ISLNK (stat_buf->st_mode))
	      {
		linkname = get_link_name (pathname, state.rel_pathname);
		if (linkname == 0)
		  state.exit_status = 1;
	      }
	    if (linkname)
	      {
		fprintf (fp, segment->text, linkname);
		free (linkname);
	      }
	    else
	      fprintf (fp, segment->text, "");
	  }
#endif				/* S_ISLNK */
	  break;
	  
	case 'M':		/* mode as 10 chars (eg., "-rwxr-x--x" */
	  {
	    char modestring[16] ;
	    mode_string (stat_buf->st_mode, modestring);
	    modestring[10] = '\0';
	    fprintf (fp, segment->text, modestring);
	  }
	  break;
	  
	case 'm':		/* mode as octal number (perms only) */
	  {
	    /* Output the mode portably using the traditional numbers,
	       even if the host unwisely uses some other numbering
	       scheme.  But help the compiler in the common case where
	       the host uses the traditional numbering scheme.  */
	    mode_t m = stat_buf->st_mode;
	    boolean traditional_numbering_scheme =
	      (S_ISUID == 04000 && S_ISGID == 02000 && S_ISVTX == 01000
	       && S_IRUSR == 00400 && S_IWUSR == 00200 && S_IXUSR == 00100
	       && S_IRGRP == 00040 && S_IWGRP == 00020 && S_IXGRP == 00010
	       && S_IROTH == 00004 && S_IWOTH == 00002 && S_IXOTH == 00001);
	    fprintf (fp, segment->text,
		     (traditional_numbering_scheme
		      ? m & MODE_ALL
		      : ((m & S_ISUID ? 04000 : 0)
			 | (m & S_ISGID ? 02000 : 0)
			 | (m & S_ISVTX ? 01000 : 0)
			 | (m & S_IRUSR ? 00400 : 0)
			 | (m & S_IWUSR ? 00200 : 0)
			 | (m & S_IXUSR ? 00100 : 0)
			 | (m & S_IRGRP ? 00040 : 0)
			 | (m & S_IWGRP ? 00020 : 0)
			 | (m & S_IXGRP ? 00010 : 0)
			 | (m & S_IROTH ? 00004 : 0)
			 | (m & S_IWOTH ? 00002 : 0)
			 | (m & S_IXOTH ? 00001 : 0))));
	  }
	  break;
	  
	case 'n':		/* number of links */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) stat_buf->st_nlink,
				   hbuf,
				   human_ceiling,
				   1, 1));
	  break;
	case 'p':		/* pathname */
	  fprintf (fp, segment->text, pathname);
	  break;
	case 'P':		/* pathname with ARGV element stripped */
	  if (state.curdepth > 0)
	    {
	      cp = pathname + state.path_length;
	      if (*cp == '/')
		/* Move past the slash between the ARGV element
		   and the rest of the pathname.  But if the ARGV element
		   ends in a slash, we didn't add another, so we've
		   already skipped past it.  */
		cp++;
	    }
	  else
	    cp = "";
	  fprintf (fp, segment->text, cp);
	  break;
	case 's':		/* size in bytes */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) stat_buf->st_size,
				   hbuf, human_ceiling, 1, 1));
	  break;
	case 't':		/* mtime in `ctime' format */
	  fprintf (fp, segment->text, ctime_format (stat_buf->st_mtime));
	  break;
	case 'u':		/* user name */
	  {
	    struct passwd *p;

	    p = getpwuid (stat_buf->st_uid);
	    if (p)
	      {
		segment->text[segment->text_len] = 's';
		fprintf (fp, segment->text, p->pw_name);
		break;
	      }
	    /* else fallthru */
	  }
	  
	case 'U':		/* UID number */
	  fprintf (fp, segment->text,
		   human_readable ((uintmax_t) stat_buf->st_uid, hbuf,
				   human_ceiling, 1, 1));
	  break;

	/* type of filesystem entry like `ls -l`: (d,-,l,s,p,b,c,n) n=nonexistent(symlink) */
	case 'Y':		/* in case of symlink */
	  {
#ifdef S_ISLNK
	  if (S_ISLNK (stat_buf->st_mode))
	    {
	      struct stat sbuf;
	      int (*ystat) ();
	      ystat = options.xstat == lstat ? stat : lstat;
	      if ((*ystat) (state.rel_pathname, &sbuf) != 0)
	      {
		if ( errno == ENOENT ) {
		  fprintf (fp, segment->text, "N");
		  break;
		};
		if ( errno == ELOOP ) {
		  fprintf (fp, segment->text, "L");
		  break;
		};
		error (0, errno, "%s", pathname);
		/* exit_status = 1;
		return (false); */
	      }
	      stat_buf->st_mode = sbuf.st_mode;
	    }
#endif /* S_ISLNK */
	  }
	  /* FALLTHROUGH */
	case 'y':
	  {
	    mode_t m = stat_buf->st_mode & S_IFMT;

	    fprintf (fp, segment->text,
		( m == S_IFSOCK ? "s" :
		  m == S_IFLNK  ? "l" :
		  m == S_IFREG  ? "f" :
		  m == S_IFBLK  ? "b" :
		  m == S_IFDIR  ? "d" :
		  m == S_IFCHR  ? "c" :
#ifdef S_IFDOOR
		  m == S_IFDOOR ? "D" :
#endif
		  m == S_IFIFO  ? "p" : "U" ) );

	  }
	  break;
	}
    }
  return (true);
}

boolean
pred_fstype (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  if (strcmp (filesystem_type (stat_buf), pred_ptr->args.str) == 0)
    return true;
  else
    return false;
}

boolean
pred_gid (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  switch (pred_ptr->args.info.kind)
    {
    case COMP_GT:
      if (stat_buf->st_gid > pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_LT:
      if (stat_buf->st_gid < pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_EQ:
      if (stat_buf->st_gid == pred_ptr->args.info.l_val)
	return (true);
      break;
    }
  return (false);
}

boolean
pred_group (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  if (pred_ptr->args.gid == stat_buf->st_gid)
    return (true);
  else
    return (false);
}

boolean
pred_ilname (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  return insert_lname (pathname, stat_buf, pred_ptr, true);
}

boolean
pred_iname (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  const char *base;

  (void) stat_buf;

  /* FNM_PERIOD is not used here because POSIX requires that it not be.
   * See http://standards.ieee.org/reading/ieee/interp/1003-2-92_int/pasc-1003.2-126.html
   */
  base = base_name (pathname);
  if (fnmatch (pred_ptr->args.str, base, FNM_CASEFOLD) == 0)
    return (true);
  return (false);
}

boolean
pred_inum (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  switch (pred_ptr->args.info.kind)
    {
    case COMP_GT:
      if (stat_buf->st_ino > pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_LT:
      if (stat_buf->st_ino < pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_EQ:
      if (stat_buf->st_ino == pred_ptr->args.info.l_val)
	return (true);
      break;
    }
  return (false);
}

boolean
pred_ipath (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) stat_buf;
  
  if (fnmatch (pred_ptr->args.str, pathname, FNM_CASEFOLD) == 0)
    return (true);
  return (false);
}

boolean
pred_links (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  switch (pred_ptr->args.info.kind)
    {
    case COMP_GT:
      if (stat_buf->st_nlink > pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_LT:
      if (stat_buf->st_nlink < pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_EQ:
      if (stat_buf->st_nlink == pred_ptr->args.info.l_val)
	return (true);
      break;
    }
  return (false);
}

boolean
pred_lname (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  return insert_lname (pathname, stat_buf, pred_ptr, false);
}

static boolean
insert_lname (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr, boolean ignore_case)
{
  boolean ret = false;
#ifdef S_ISLNK
  if (S_ISLNK (stat_buf->st_mode))
    {
      char *linkname = get_link_name (pathname, state.rel_pathname);
      if (linkname)
	{
	  if (fnmatch (pred_ptr->args.str, linkname,
		       ignore_case ? FNM_CASEFOLD : 0) == 0)
	    ret = true;
	  free (linkname);
	}
    }
#endif /* S_ISLNK */
  return (ret);
}

boolean
pred_ls (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pred_ptr;
  
  list_file (pathname, state.rel_pathname, stat_buf, options.start_time,
	     options.output_block_size, stdout);
  return (true);
}

boolean
pred_mmin (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) &pathname;
  return pred_timewindow(stat_buf->st_mtime, pred_ptr, 60);
}

boolean
pred_mtime (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  return pred_timewindow(stat_buf->st_mtime, pred_ptr, DAYSECS);
}

boolean
pred_name (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  const char *base;

  (void) stat_buf;
  base = base_name (pathname);

  /* FNM_PERIOD is not used here because POSIX requires that it not be.
   * See http://standards.ieee.org/reading/ieee/interp/1003-2-92_int/pasc-1003.2-126.html
   */
  if (fnmatch (pred_ptr->args.str, base, 0) == 0)
    return (true);
  return (false);
}

boolean
pred_negate (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  /* Check whether we need a stat here. */
  /* TODO: what about need_type? */
  if (get_info(pathname, state.rel_pathname, stat_buf, pred_ptr) != 0)
    return false;
  return (!(*pred_ptr->pred_right->pred_func) (pathname, stat_buf,
					      pred_ptr->pred_right));
}

boolean
pred_newer (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  
  if (stat_buf->st_mtime > pred_ptr->args.time)
    return (true);
  return (false);
}

boolean
pred_nogroup (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  (void) pred_ptr;
  
#ifdef CACHE_IDS
  extern char *gid_unused;

  return gid_unused[(unsigned) stat_buf->st_gid];
#else
  return getgrgid (stat_buf->st_gid) == NULL;
#endif
}

boolean
pred_nouser (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
#ifdef CACHE_IDS
  extern char *uid_unused;
#endif
  
  (void) pathname;
  (void) pred_ptr;
  
#ifdef CACHE_IDS
  return uid_unused[(unsigned) stat_buf->st_uid];
#else
  return getpwuid (stat_buf->st_uid) == NULL;
#endif
}

#if defined(NEW_EXEC)

static boolean
is_ok(const char *program, const char *arg)
{
  fflush (stdout);
  /* The draft open standard requires that, in the POSIX locale,
     the last non-blank character of this prompt be '?'.
     The exact format is not specified.
     This standard does not have requirements for locales other than POSIX
  */
  fprintf (stderr, _("< %s ... %s > ? "), program, arg);
  fflush (stderr);
  return yesno();
}

boolean
pred_ok (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  if (is_ok(pred_ptr->args.exec_vec.replace_vec[0], pathname))
    return new_impl_pred_exec (pathname, stat_buf, pred_ptr, NULL, 0);
  else
    return false;
}

boolean
pred_okdir (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  const char *s = basename(pathname);
  
  if (is_ok(pred_ptr->args.exec_vec.replace_vec[0], pathname))
    return new_impl_pred_exec (s, stat_buf, pred_ptr, "./", 2);
  else
    return false;
}



#else
boolean
pred_ok (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  fflush (stdout);
  /* The draft open standard requires that, in the POSIX locale,
     the last non-blank character of this prompt be '?'.
     The exact format is not specified.
     This standard does not have requirements for locales other than POSIX
  */
  fprintf (stderr, _("< %s ... %s > ? "),
	   pred_ptr->args.exec_vec.vec[0], pathname);
  fflush (stderr);
  if (yesno ())
    return pred_exec (pathname, stat_buf, pred_ptr);
  else
    return (false);
}

boolean
pred_okdir (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  error(1, 0, "-okdir is not yet implemented.");
  return false;
}


#endif


boolean
pred_open (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  (void) stat_buf;
  (void) pred_ptr;
  return true;
}

boolean
pred_or (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  if (pred_ptr->pred_left == NULL
      || !(*pred_ptr->pred_left->pred_func) (pathname, stat_buf,
					     pred_ptr->pred_left))
    {
      if (get_info(pathname, state.rel_pathname, stat_buf, pred_ptr) != 0)
	return false;
      return ((*pred_ptr->pred_right->pred_func) (pathname, stat_buf,
						  pred_ptr->pred_right));
    }
  else
    return true;
}

boolean
pred_path (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) stat_buf;
  if (fnmatch (pred_ptr->args.str, pathname, 0) == 0)
    return (true);
  return (false);
}

boolean
pred_perm (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  switch (pred_ptr->args.perm.kind)
    {
    case PERM_AT_LEAST:
      return (stat_buf->st_mode & pred_ptr->args.perm.val) == pred_ptr->args.perm.val;
      break;

    case PERM_ANY:
      return (stat_buf->st_mode & pred_ptr->args.perm.val) != 0;
      break;

    case PERM_EXACT:
      return (stat_buf->st_mode & MODE_ALL) == pred_ptr->args.perm.val;
      break;

    default:
      abort ();
      break;
    }
}

boolean
pred_print (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) stat_buf;
  (void) pred_ptr;
  puts (pathname);
  return true;
}

boolean
pred_print0 (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) stat_buf;
  (void) pred_ptr;
  fputs (pathname, stdout);
  putc (0, stdout);
  return (true);
}

boolean
pred_prune (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  (void) stat_buf;
  (void) pred_ptr;
  state.stop_at_current_level = true;
  return (options.do_dir_first); /* This is what SunOS find seems to do. */
}

boolean
pred_quit (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  (void) stat_buf;
  (void) pred_ptr;

  /* Since -exec and friends don't leave child processes running in the 
   * background, there is no need to wait for them here.
   */
  exit(state.exit_status);	/* 0 for success, etc. */
}

boolean
pred_regex (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  int len = strlen (pathname);
(void) stat_buf;
  if (re_match (pred_ptr->args.regex, pathname, len, 0,
		(struct re_registers *) NULL) == len)
    return (true);
  return (false);
}

boolean
pred_size (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  uintmax_t f_val;

  (void) pathname;
  f_val = ((stat_buf->st_size / pred_ptr->args.size.blocksize)
	   + (stat_buf->st_size % pred_ptr->args.size.blocksize != 0));
  switch (pred_ptr->args.size.kind)
    {
    case COMP_GT:
      if (f_val > pred_ptr->args.size.size)
	return (true);
      break;
    case COMP_LT:
      if (f_val < pred_ptr->args.size.size)
	return (true);
      break;
    case COMP_EQ:
      if (f_val == pred_ptr->args.size.size)
	return (true);
      break;
    }
  return (false);
}

boolean
pred_samefile (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  /* Potential optimisation: because of the loop protection, we 
   * always know the device of the current directory, hence the 
   * device number of the current filesystem.  If -L is not in 
   * effect, and the device number of the file we're looking for 
   * is not the same as the device number of the current directory,
   * this predicate cannot return true.   Hence there would be no 
   * need to stat the file.
   */
  return stat_buf->st_ino == pred_ptr->args.fileid.ino
    &&   stat_buf->st_dev == pred_ptr->args.fileid.dev;
}

boolean
pred_true (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  (void) stat_buf;
  (void) pred_ptr;
  return true;
}

boolean
pred_type (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  mode_t mode;
  mode_t type = pred_ptr->args.type;

  assert(state.have_type);
  assert(state.type != 0);
  
  (void) pathname;

  if (state.have_stat)
     mode = stat_buf->st_mode;
  else
     mode = state.type;

#ifndef S_IFMT
  /* POSIX system; check `mode' the slow way. */
  if ((S_ISBLK (mode) && type == S_IFBLK)
      || (S_ISCHR (mode) && type == S_IFCHR)
      || (S_ISDIR (mode) && type == S_IFDIR)
      || (S_ISREG (mode) && type == S_IFREG)
#ifdef S_IFLNK
      || (S_ISLNK (mode) && type == S_IFLNK)
#endif
#ifdef S_IFIFO
      || (S_ISFIFO (mode) && type == S_IFIFO)
#endif
#ifdef S_IFSOCK
      || (S_ISSOCK (mode) && type == S_IFSOCK)
#endif
#ifdef S_IFDOOR
      || (S_ISDOOR (mode) && type == S_IFDOOR)
#endif
      )
#else /* S_IFMT */
  /* Unix system; check `mode' the fast way. */
  if ((mode & S_IFMT) == type)
#endif /* S_IFMT */
    return (true);
  else
    return (false);
}

boolean
pred_uid (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  switch (pred_ptr->args.info.kind)
    {
    case COMP_GT:
      if (stat_buf->st_uid > pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_LT:
      if (stat_buf->st_uid < pred_ptr->args.info.l_val)
	return (true);
      break;
    case COMP_EQ:
      if (stat_buf->st_uid == pred_ptr->args.info.l_val)
	return (true);
      break;
    }
  return (false);
}

boolean
pred_used (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  time_t delta;

  (void) pathname;
  delta = stat_buf->st_atime - stat_buf->st_ctime; /* Use difftime? */
  return pred_timewindow(delta, pred_ptr, DAYSECS);
}

boolean
pred_user (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  (void) pathname;
  if (pred_ptr->args.uid == stat_buf->st_uid)
    return (true);
  else
    return (false);
}

boolean
pred_xtype (char *pathname, struct stat *stat_buf, struct predicate *pred_ptr)
{
  struct stat sbuf;		/* local copy, not stat_buf because we're using a different stat method */
  int (*ystat) (const char*, struct stat *p);

  switch (options.symlink_handling)
    {
    case SYMLINK_ALWAYS_DEREF:
      ystat = optionl_stat;
    case SYMLINK_DEREF_ARGSONLY:
    case SYMLINK_NEVER_DEREF:
      ystat = optionp_stat;
    }
  
  if ((*ystat) (state.rel_pathname, &sbuf) != 0)
    {
      if (ystat == optionl_stat && errno == ENOENT)
	{
	  /* If we failed to follow the symlink,
	   * fall back on looking at the symlink itself. 
	   */
	  /* Mimic behavior of ls -lL. */
	  return (pred_type (pathname, stat_buf, pred_ptr));
	}
      else
	{
	  error (0, errno, "%s", pathname);
	  state.exit_status = 1;
	}
      return false;
    }
  /* Now that we have our stat() information, query it in the same 
   * way that -type does.
   */
  return (pred_type (pathname, &sbuf, pred_ptr));
}

/*  1) fork to get a child; parent remembers the child pid
    2) child execs the command requested
    3) parent waits for child; checks for proper pid of child

    Possible returns:

    ret		errno	status(h)   status(l)

    pid		x	signal#	    0177	stopped
    pid		x	exit arg    0		term by _exit
    pid		x	0	    signal #	term by signal
    -1		EINTR				parent got signal
    -1		other				some other kind of error

    Return true only if the pid matches, status(l) is
    zero, and the exit arg (status high) is 0.
    Otherwise return false, possibly printing an error message. */

#if defined(NEW_EXEC)
static void
prep_child_for_exec (void)
{
  const char inputfile[] = "/dev/null";
  /* fprintf(stderr, "attaching stdin to /dev/null\n"); */
  
  close(0);
  if (open(inputfile, O_RDONLY) < 0)
    {
      /* This is not entirely fatal, since 
       * executing the child with a closed
       * stdin is almost as good as executing it
       * with its stdin attached to /dev/null.
       */
      error (0, errno, "%s", inputfile);
    }
}



int
launch (const struct buildcmd_control *ctl,
	struct buildcmd_state *buildstate)
{
  int wait_status;
  pid_t child_pid;
  static int first_time = 1;
  const struct exec_val *execp = buildstate->usercontext;
  
  /* Null terminate the arg list.  */
  bc_push_arg (ctl, buildstate, (char *) NULL, 0, NULL, 0, false); 
  
  /* Make sure output of command doesn't get mixed with find output. */
  fflush (stdout);
  fflush (stderr);
  
  /* Make sure to listen for the kids.  */
  if (first_time)
    {
      first_time = 0;
      signal (SIGCHLD, SIG_DFL);
    }

  child_pid = fork ();
  if (child_pid == -1)
    error (1, errno, _("cannot fork"));
  if (child_pid == 0)
    {
      /* We be the child. */
      prep_child_for_exec();

      /* For -exec and -ok, change directory back to the starting directory.
       * for -execdir and -okdir, stay in the directory we are searching
       * (the latter is more secure).
       */
      if (!execp->use_current_dir)
	{
	  if (starting_desc < 0
	      ? chdir (starting_dir) != 0
	      : fchdir (starting_desc) != 0)
	    {
	      error (0, errno, "%s", starting_dir);
	      _exit (1);
	    }
	}
      
      execvp (buildstate->cmd_argv[0], buildstate->cmd_argv);
      error (0, errno, "%s", buildstate->cmd_argv[0]);
      _exit (1);
    }


  /* In parent; set up for next time. */
  bc_clear_args(ctl, buildstate);

  
  while (waitpid (child_pid, &wait_status, 0) == (pid_t) -1)
    {
      if (errno != EINTR)
	{
	  error (0, errno, _("error waiting for %s"), buildstate->cmd_argv[0]);
	  state.exit_status = 1;
	  return 0;		/* FAIL */
	}
    }
  
  if (WIFSIGNALED (wait_status))
    {
      error (0, 0, _("%s terminated by signal %d"),
	     buildstate->cmd_argv[0], WTERMSIG (wait_status));
      state.exit_status = 1;
      return 1;			/* OK */
    }

  if (0 == WEXITSTATUS (wait_status))
    {
      return 1;			/* OK */
    }
  else
    {
      
      state.exit_status = 1;
      return 0;			/* FAIL */
    }
  
}
#else
static boolean
launch (struct predicate *pred_ptr)
{
  int status;
  pid_t child_pid;
  struct exec_val *execp;	/* Pointer for efficiency. */
  static int first_time = 1;

  execp = &pred_ptr->args.exec_vec;

  /* Make sure output of command doesn't get mixed with find output. */
  fflush (stdout);
  fflush (stderr);

  /* Make sure to listen for the kids.  */
  if (first_time)
    {
      first_time = 0;
      signal (SIGCHLD, SIG_DFL);
    }

  child_pid = fork ();
  if (child_pid == -1)
    error (1, errno, _("cannot fork"));
  if (child_pid == 0)
    {
      /* We be the child. */
      if (starting_desc < 0
	  ? chdir (starting_dir) != 0
	  : fchdir (starting_desc) != 0)
	{
	  error (0, errno, "%s", starting_dir);
	  _exit (1);
	}
      execvp (execp->vec[0], execp->vec);
      error (0, errno, "%s", execp->vec[0]);
      _exit (1);
    }

  
  while (waitpid (child_pid, &status, 0) == (pid_t) -1)
    if (errno != EINTR)
      {
	error (0, errno, _("error waiting for %s"), execp->vec[0]);
	state.exit_status = 1;
	return false;
      }
  if (WIFSIGNALED (status))
    {
      error (0, 0, _("%s terminated by signal %d"),
	     execp->vec[0], WTERMSIG (status));
      state.exit_status = 1;
      return (false);
    }
  return (!WEXITSTATUS (status));
}
#endif


/* Return a static string formatting the time WHEN according to the
   strftime format character KIND.  */

static char *
format_date (time_t when, int kind)
{
  static char buf[MAX (LONGEST_HUMAN_READABLE + 2, 64)];
  struct tm *tm;
  char fmt[6];

  fmt[0] = '%';
  fmt[1] = kind;
  fmt[2] = '\0';
  if (kind == '+')
    strcpy (fmt, "%F+%T");

  if (kind != '@'
      && (tm = localtime (&when))
      && strftime (buf, sizeof buf, fmt, tm))
    return buf;
  else
    {
      uintmax_t w = when;
      char *p = human_readable (when < 0 ? -w : w, buf + 1,
				human_ceiling, 1, 1);
      if (when < 0)
	*--p = '-';
      return p;
    }
}

static char *
ctime_format (when)
     time_t when;
{
  char *r = ctime (&when);
  if (!r)
    {
      /* The time cannot be represented as a struct tm.
	 Output it as an integer.  */
      return format_date (when, '@');
    }
  else
    {
      /* Remove the trailing newline from the ctime output,
	 being careful not to assume that the output is fixed-width.  */
      *strchr (r, '\n') = '\0';
      return r;
    }
}

#ifdef	DEBUG
/* Return a pointer to the string representation of 
   the predicate function PRED_FUNC. */

char *
find_pred_name (pred_func)
     PFB pred_func;
{
  int i;

  for (i = 0; pred_table[i].pred_func != 0; i++)
    if (pred_table[i].pred_func == pred_func)
      break;
  return (pred_table[i].pred_name);
}

static char *
type_name (type)
     short type;
{
  int i;

  for (i = 0; type_table[i].type != (short) -1; i++)
    if (type_table[i].type == type)
      break;
  return (type_table[i].type_name);
}

static char *
prec_name (prec)
     short prec;
{
  int i;

  for (i = 0; prec_table[i].prec != (short) -1; i++)
    if (prec_table[i].prec == prec)
      break;
  return (prec_table[i].prec_name);
}

/* Walk the expression tree NODE to stdout.
   INDENT is the number of levels to indent the left margin. */

void
print_tree (FILE *fp, struct predicate *node, int indent)
{
  int i;

  if (node == NULL)
    return;
  for (i = 0; i < indent; i++)
    fprintf (fp, "    ");
  fprintf (fp, "pred = %s type = %s prec = %s addr = %p\n",
	  find_pred_name (node->pred_func),
	  type_name (node->p_type), prec_name (node->p_prec), node);
  if (node->need_stat || node->need_type)
    {
      int comma = 0;
      
      for (i = 0; i < indent; i++)
	fprintf (fp, "    ");
      
      fprintf (fp, "Needs ");
      if (node->need_stat)
	{
	  fprintf (fp, "stat");
	  comma = 1;
	}
      if (node->need_type)
	{
	  fprintf (fp, "%stype", comma ? "," : "");
	}
      fprintf (fp, "\n");
    }
  
  for (i = 0; i < indent; i++)
    fprintf (fp, "    ");
  fprintf (fp, _("left:\n"));
  print_tree (fp, node->pred_left, indent + 1);
  for (i = 0; i < indent; i++)
    fprintf (fp, "    ");
  fprintf (fp, _("right:\n"));
  print_tree (fp, node->pred_right, indent + 1);
}

/* Copy STR into BUF and trim blanks from the end of BUF.
   Return BUF. */

static char *
blank_rtrim (str, buf)
     char *str;
     char *buf;
{
  int i;

  if (str == NULL)
    return (NULL);
  strcpy (buf, str);
  i = strlen (buf) - 1;
  while ((i >= 0) && ((buf[i] == ' ') || buf[i] == '\t'))
    i--;
  buf[++i] = '\0';
  return (buf);
}

/* Print out the predicate list starting at NODE. */

void
print_list (FILE *fp, struct predicate *node)
{
  struct predicate *cur;
  char name[256];

  cur = node;
  while (cur != NULL)
    {
      fprintf (fp, "%s ", blank_rtrim (find_pred_name (cur->pred_func), name));
      cur = cur->pred_next;
    }
  fprintf (fp, "\n");
}


/* Print out the predicate list starting at NODE. */


static void
print_parenthesised(FILE *fp, struct predicate *node)
{
  int parens = 0;

  if (node)
    {
      if ( ( (node->pred_func == pred_or)
	     || (node->pred_func == pred_and) )
	  && node->pred_left == NULL)
	{
	  /* We print "<nothing> or  X" as just "X"
	   * We print "<nothing> and X" as just "X"
	   */
	  print_parenthesised(fp, node->pred_right);
	}
      else
	{
	  if (node->pred_left || node->pred_right)
	    parens = 1;

	  if (parens)
	    fprintf(fp, "%s", " ( ");
	  print_optlist(fp, node);
	  if (parens)
	    fprintf(fp, "%s", " ) ");
	}
    }
}

void
print_optlist (FILE *fp, struct predicate *p)
{
  char name[256];

  if (p)
    {
      print_parenthesised(fp, p->pred_left);
      fprintf (fp,
	       "%s%s%s ",
	       p->need_stat ? _("[stat called here] ") : "",
	       p->need_type ? _("[type needed here] ") : "",
	       blank_rtrim (find_pred_name (p->pred_func), name));
      print_parenthesised(fp, p->pred_right);
    }
}

#endif	/* DEBUG */
