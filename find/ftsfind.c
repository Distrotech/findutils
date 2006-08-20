/* find -- search for files in a directory hierarchy (fts version)
   Copyright (C) 1990, 91, 92, 93, 94, 2000, 
                 2003, 2004, 2005 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.*/

/* This file was written by James Youngman, based on find.c.
   
   GNU find was written by Eric Decker <cire@cisco.com>,
   with enhancements by David MacKenzie <djm@gnu.org>,
   Jay Plett <jay@silence.princeton.nj.us>,
   and Tim Wood <axolotl!tim@toad.com>.
   The idea for -print0 and xargs -0 came from
   Dan Bernstein <brnstnd@kramden.acf.nyu.edu>.  
*/


#include "defs.h"


#define USE_SAFE_CHDIR 1
#undef  STAT_MOUNTPOINTS


#include <errno.h>
#include <assert.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif


#include "../gnulib/lib/xalloc.h"
#include "closeout.h"
#include <modetype.h>
#include "quotearg.h"
#include "quote.h"
#include "fts_.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)
#endif
#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
/* See locate.c for explanation as to why not use (String) */
# define N_(String) String
#endif


#ifdef STAT_MOUNTPOINTS
static void init_mounted_dev_list(void);
#endif

/* We have encountered an error which shoudl affect the exit status.
 * This is normally used to change the exit status from 0 to 1.
 * However, if the exit status is already 2 for example, we don't want to 
 * reduce it to 1.
 */
static void
error_severity(int level)
{
  if (state.exit_status < level)
    state.exit_status = level;
}


#define STRINGIFY(X) #X
#define HANDLECASE(N) case N: return #N;

static char *
get_fts_info_name(int info)
{
  static char buf[10];
  switch (info)
    {
      HANDLECASE(FTS_D);
      HANDLECASE(FTS_DC);
      HANDLECASE(FTS_DEFAULT);
      HANDLECASE(FTS_DNR);
      HANDLECASE(FTS_DOT);
      HANDLECASE(FTS_DP);
      HANDLECASE(FTS_ERR);
      HANDLECASE(FTS_F);
      HANDLECASE(FTS_INIT);
      HANDLECASE(FTS_NS);
      HANDLECASE(FTS_NSOK);
      HANDLECASE(FTS_SL);
      HANDLECASE(FTS_SLNONE);
      HANDLECASE(FTS_W);
    default:
      sprintf(buf, "[%d]", info);
      return buf;
    }
}

static void
visit(FTS *p, FTSENT *ent, struct stat *pstat)
{
  struct predicate *eval_tree;
  
  state.curdepth = ent->fts_level;
  state.have_stat = (ent->fts_info != FTS_NS) && (ent->fts_info != FTS_NSOK);
  state.rel_pathname = ent->fts_accpath;

  /* Apply the predicates to this path. */
  eval_tree = get_eval_tree();
  (*(eval_tree)->pred_func)(ent->fts_path, pstat, eval_tree);

  /* Deal with any side effects of applying the predicates. */
  if (state.stop_at_current_level)
    {
      fts_set(p, ent, FTS_SKIP);
    }
}

static const char*
partial_quotearg_n(int n, char *s, size_t len, enum quoting_style style)
{
  if (0 == len)
    {
      return quotearg_n_style(n, style, "");
    }
  else
    {
      char saved;
      const char *result;
      
      saved = s[len];
      s[len] = 0;
      result = quotearg_n_style(n, style, s);
      s[len] = saved;
      return result;
    }
}


/* We've detected a filesystem loop.   This is caused by one of 
 * two things:
 *
 * 1. Option -L is in effect and we've hit a symbolic link that 
 *    points to an ancestor.  This is harmless.  We won't traverse the 
 *    symbolic link.
 *
 * 2. We have hit a real cycle in the directory hierarchy.  In this 
 *    case, we issue a diagnostic message (POSIX requires this) and we
 *    skip that directory entry.
 */
static void
issue_loop_warning(FTSENT * ent)
{
  if (S_ISLNK(ent->fts_statp->st_mode))
    {
      error(0, 0,
	    _("Symbolic link %s is part of a loop in the directory hierarchy; we have already visited the directory to which it points."),
	    quotearg_n_style(0, locale_quoting_style, ent->fts_path));
    }
  else
    {
      /* We have found an infinite loop.  POSIX requires us to
       * issue a diagnostic.  Usually we won't get to here
       * because when the leaf optimisation is on, it will cause
       * the subdirectory to be skipped.  If /a/b/c/d is a hard
       * link to /a/b, then the link count of /a/b/c is 2,
       * because the ".." entry of /b/b/c/d points to /a, not
       * to /a/b/c.
       */
      error(0, 0,
	    _("Filesystem loop detected; "
	      "%s is part of the same filesystem loop as %s."),
	    quotearg_n_style(0, locale_quoting_style, ent->fts_path),
	    partial_quotearg_n(1,
			       ent->fts_cycle->fts_path,
			       ent->fts_cycle->fts_pathlen,
			       locale_quoting_style));
    }
}

/* 
 * Return true if NAME corresponds to a file which forms part of a 
 * symbolic link loop.  The command 
 *      rm -f a b; ln -s a b; ln -s b a 
 * produces such a loop.
 */
static boolean 
symlink_loop(const char *name)
{
  struct stat stbuf;
  int rv;
  if (following_links())
    rv = stat(name, &stbuf);
  else
    rv = lstat(name, &stbuf);
  return (0 != rv) && (ELOOP == errno);
}


static void
consider_visiting(FTS *p, FTSENT *ent)
{
  struct stat statbuf;
  mode_t mode;
  int ignore, isdir;

  if (options.debug_options & DebugSearch)
    fprintf(stderr,
	    "consider_visiting: fts_info=%-6s, fts_level=%2d, "
            "fts_path=%s\n",
	    get_fts_info_name(ent->fts_info),
            (int)ent->fts_level,
	    quotearg_n_style(0, locale_quoting_style, ent->fts_path));

  /* Cope with various error conditions. */
  if (ent->fts_info == FTS_ERR
      || ent->fts_info == FTS_NS
      || ent->fts_info == FTS_DNR)
    {
      error(0, ent->fts_errno, ent->fts_path);
      error_severity(1);
      return;
    }
  else if (ent->fts_info == FTS_DC)
    {
      issue_loop_warning(ent);
      error_severity(1);
      return;
    }
  else if (ent->fts_info == FTS_SLNONE)
    {
      /* fts_read() claims that ent->fts_accpath is a broken symbolic
       * link.  That would be fine, but if this is part of a symbolic
       * link loop, we diagnose the problem and also ensure that the
       * eventual return value is nonzero.   Note that while the path 
       * we stat is local (fts_accpath), we print the fill path name 
       * of the file (fts_path) in the error message.
       */
      if (symlink_loop(ent->fts_accpath))
	{
	  error(0, ELOOP, ent->fts_path);
	  error_severity(1);
	  return;
	}
    }
  
  /* Not an error, cope with the usual cases. */
  if (ent->fts_info == FTS_NSOK)
    {
      assert(!state.have_stat);
      assert(!state.have_type);
      state.type = mode = 0;
    }
  else
    {
      state.have_stat = true;
      state.have_type = true;
      statbuf = *(ent->fts_statp);
      state.type = mode = statbuf.st_mode;
    }

  if (0 == ent->fts_level && (0u == state.starting_path_length))
    state.starting_path_length = ent->fts_pathlen;

  if (mode)
    {
      if (!digest_mode(mode, ent->fts_path, ent->fts_name, &statbuf, 0))
	return;
    }

  /* examine this item. */
  ignore = 0;
  isdir = S_ISDIR(statbuf.st_mode)
    || (FTS_D  == ent->fts_info)
    || (FTS_DP == ent->fts_info)
    || (FTS_DC == ent->fts_info);

  if (isdir && (ent->fts_info == FTS_NSOK))
    {
      /* This is a directory, but fts did not stat it, so
       * presumably would not be planning to search its
       * children.  Force a stat of the file so that the
       * children can be checked.
       */
      fts_set(p, ent, FTS_AGAIN);
      return;
    }

  if (options.maxdepth >= 0)
    {
      if (ent->fts_level >= options.maxdepth)
	{
	  fts_set(p, ent, FTS_SKIP); /* descend no further */
	  
	  if (ent->fts_level > options.maxdepth) 
	    ignore = 1;		/* don't even look at this one */
	}
    }

  if ( (ent->fts_info == FTS_D) && !options.do_dir_first )
    {
      /* this is the preorder visit, but user said -depth */ 
      ignore = 1;
    }
  else if ( (ent->fts_info == FTS_DP) && options.do_dir_first )
    {
      /* this is the postorder visit, but user didn't say -depth */ 
      ignore = 1;
    }
  else if (ent->fts_level < options.mindepth)
    {
      ignore = 1;
    }

  if (!ignore)
    {
      visit(p, ent, &statbuf);
    }


  if (ent->fts_info == FTS_DP)
    {
      /* we're leaving a directory. */
      state.stop_at_current_level = false;
      complete_pending_execdirs(get_eval_tree());
    }
}


static void
find(char *arg)
{
  char * arglist[2];
  int ftsoptions;
  FTS *p;
  FTSENT *ent;
  

  arglist[0] = arg;
  arglist[1] = NULL;
  
  ftsoptions = FTS_NOSTAT;
  switch (options.symlink_handling)
    {
    case SYMLINK_ALWAYS_DEREF:
      ftsoptions |= FTS_COMFOLLOW|FTS_LOGICAL;
      break;
	  
    case SYMLINK_DEREF_ARGSONLY:
      ftsoptions |= FTS_COMFOLLOW|FTS_PHYSICAL;
      break;
	  
    case SYMLINK_NEVER_DEREF:
      ftsoptions |= FTS_PHYSICAL;
      break;
    }

  if (options.stay_on_filesystem)
    ftsoptions |= FTS_XDEV;
      
  p = fts_open(arglist, ftsoptions, NULL);
  if (NULL == p)
    {
      error (0, errno,
	     _("cannot search %s"),
	     quotearg_n_style(0, locale_quoting_style, arg));
    }
  else
    {
      while ( (ent=fts_read(p)) != NULL )
	{
	  state.have_stat = false;
	  state.have_type = false;
	  state.type = 0;
	  
	  consider_visiting(p, ent);
	}
      fts_close(p);
      p = NULL;
    }
}


static void 
process_all_startpoints(int argc, char *argv[])
{
  int i;

  /* figure out how many start points there are */
  for (i = 0; i < argc && !looks_like_expression(argv[i], true); i++)
    {
      find(argv[i]);
    }
  
  if (i == 0)
    {
      /* 
       * We use a temporary variable here because some actions modify 
       * the path temporarily.  Hence if we use a string constant, 
       * we get a coredump.  The best example of this is if we say 
       * "find -printf %H" (note, not "find . -printf %H").
       */
      char defaultpath[2] = ".";
      find(defaultpath);
    }
}




int
main (int argc, char **argv)
{
  int end_of_leading_options = 0; /* First arg after any -H/-L etc. */
  struct predicate *eval_tree;

  program_name = argv[0];
  state.exit_status = 0;


  /* Set the option defaults before we do the the locale
   * initialisation as check_nofollow() needs to be executed in the
   * POSIX locale.
   */
  set_option_defaults(&options);
  
#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  atexit (close_stdout);

  /* Check for -P, -H or -L options.  Also -D and -O, which are 
   * both GNU extensions.
   */
  end_of_leading_options = process_leading_options(argc, argv);
  
  if (options.debug_options & DebugStat)
    options.xstat = debug_stat;

#ifdef DEBUG
  fprintf (stderr, "cur_day_start = %s", ctime (&options.cur_day_start));
#endif /* DEBUG */


  /* We are now processing the part of the "find" command line 
   * after the -H/-L options (if any).
   */
  eval_tree = build_expression_tree(argc, argv, end_of_leading_options);

  /* safely_chdir() needs to check that it has ended up in the right place. 
   * To avoid bailing out when something gets automounted, it checks if 
   * the target directory appears to have had a directory mounted on it as
   * we chdir()ed.  The problem with this is that in order to notice that 
   * a filesystem was mounted, we would need to lstat() all the mount points.
   * That strategy loses if our machine is a client of a dead NFS server.
   *
   * Hence if safely_chdir() and wd_sanity_check() can manage without needing 
   * to know the mounted device list, we do that.  
   */
  if (!options.open_nofollow_available)
    {
#ifdef STAT_MOUNTPOINTS
      init_mounted_dev_list();
#endif
    }
  

  starting_desc = open (".", O_RDONLY);
  if (0 <= starting_desc && fchdir (starting_desc) != 0)
    {
      close (starting_desc);
      starting_desc = -1;
    }
  if (starting_desc < 0)
    {
      starting_dir = xgetcwd ();
      if (! starting_dir)
	error (1, errno, _("cannot get current directory"));
    }


  process_all_startpoints(argc-end_of_leading_options, argv+end_of_leading_options);
  
  /* If "-exec ... {} +" has been used, there may be some 
   * partially-full command lines which have been built, 
   * but which are not yet complete.   Execute those now.
   */
  cleanup();
  return state.exit_status;
}

boolean is_fts_enabled()
{
  /* this version of find (i.e. this main()) uses fts. */
  return true;
}
