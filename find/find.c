/* find -- search for files in a directory hierarchy
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
   USA.*/

/* GNU find was written by Eric Decker <cire@cisco.com>,
   with enhancements by David MacKenzie <djm@gnu.ai.mit.edu>,
   Jay Plett <jay@silence.princeton.nj.us>,
   and Tim Wood <axolotl!tim@toad.com>.
   The idea for -print0 and xargs -0 came from
   Dan Bernstein <brnstnd@kramden.acf.nyu.edu>.  */


#include "defs.h"

#define USE_SAFE_CHDIR 1

#include <errno.h>
#include <assert.h>


#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#include "../gnulib/lib/xalloc.h"
#include "../gnulib/lib/human.h"
#include "../gnulib/lib/canonicalize.h"
#include "closeout.h"
#include <modetype.h>
#include "savedirinfo.h"
#include "buildcmd.h"

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

#define apply_predicate(pathname, stat_buf_ptr, node)	\
  (*(node)->pred_func)((pathname), (stat_buf_ptr), (node))


static void init_mounted_dev_list(void);
static void process_top_path PARAMS((char *pathname, mode_t mode));
static int process_path PARAMS((char *pathname, char *name, boolean leaf, char *parent, mode_t type));
static void process_dir PARAMS((char *pathname, char *name, int pathlen, struct stat *statp, char *parent));

static void complete_pending_execdirs(struct predicate *p);
static void complete_pending_execs   (struct predicate *p);



static boolean default_prints PARAMS((struct predicate *pred));

/* Name this program was run with. */
char *program_name;

/* All predicates for each path to process. */
struct predicate *predicates;

/* The last predicate allocated. */
struct predicate *last_pred;

/* The root of the evaluation tree. */
static struct predicate *eval_tree;


struct options options;
struct state state;

/* The full path of the initial working directory, or "." if
   STARTING_DESC is nonnegative.  */
char const *starting_dir = ".";

/* A file descriptor open to the initial working directory.
   Doing it this way allows us to work when the i.w.d. has
   unreadable parents.  */
int starting_desc;

/* The stat buffer of the initial working directory. */
struct stat starting_stat_buf;


enum TraversalDirection
  {
    TraversingUp,
    TraversingDown
  };


static int
following_links(void)
{
  switch (options.symlink_handling)
    {
    case SYMLINK_ALWAYS_DEREF:
      return 1;
    case SYMLINK_DEREF_ARGSONLY:
      return (state.curdepth == 0);
    case SYMLINK_NEVER_DEREF:
    default:
      return 0;
    }
}


static int
fallback_stat(const char *name, struct stat *p, int prev_rv)
{
  /* Our original stat() call failed.  Perhaps we can't follow a
   * symbolic link.  If that might be the problem, lstat() the link. 
   * Otherwise, admit defeat. 
   */
  switch (errno)
    {
    case ENOENT:
    case ENOTDIR:
#ifdef DEBUG_STAT
      fprintf(stderr, "fallback_stat(): stat(%s) failed; falling back on lstat()\n", name);
#endif
      return lstat(name, p);

    case EACCES:
    case EIO:
    case ELOOP:
    case ENAMETOOLONG:
#ifdef EOVERFLOW
    case EOVERFLOW:	    /* EOVERFLOW is not #defined on UNICOS. */
#endif
    default:
      return prev_rv;	       
    }
}


/* optionh_stat() implements the stat operation when the -H option is
 * in effect.
 * 
 * If the item to be examined is a command-line argument, we follow
 * symbolic links.  If the stat() call fails on the command-line item,
 * we fall back on the properties of the symbolic link.
 *
 * If the item to be examined is not a command-line argument, we
 * examine the link itself.
 */
int 
optionh_stat(const char *name, struct stat *p)
{
  if (0 == state.curdepth) 
    {
      /* This file is from the command line; deference the link (if it
       * is a link).  
       */
      int rv = stat(name, p);
      if (0 == rv)
	return 0;		/* success */
      else
	return fallback_stat(name, p, rv);
    }
  else
    {
      /* Not a file on the command line; do not derefernce the link.
       */
      return lstat(name, p);
    }
}

/* optionl_stat() implements the stat operation when the -L option is
 * in effect.  That option makes us examine the thing the symbolic
 * link points to, not the symbolic link itself.
 */
int 
optionl_stat(const char *name, struct stat *p)
{
  int rv = stat(name, p);
  if (0 == rv)
    return 0;			/* normal case. */
  else
    return fallback_stat(name, p, rv);
}

/* optionp_stat() implements the stat operation when the -P option is
 * in effect (this is also the default).  That option makes us examine
 * the symbolic link itself, not the thing it points to.
 */
int 
optionp_stat(const char *name, struct stat *p)
{
  return lstat(name, p);
}

#ifdef DEBUG_STAT
static uintmax_t stat_count = 0u;

static int
debug_stat (const char *file, struct stat *bufp)
{
  ++stat_count;
  fprintf (stderr, "debug_stat (%s)\n", file);
  switch (options.symlink_handling)
    {
    case SYMLINK_ALWAYS_DEREF:
      return optionl_stat(file, bufp);
    case SYMLINK_DEREF_ARGSONLY:
      return optionh_stat(file, bufp);
    case SYMLINK_NEVER_DEREF:
      return optionp_stat(file, bufp);
    }
}
#endif /* DEBUG_STAT */

void 
set_follow_state(enum SymlinkOption opt)
{
  switch (opt)
    {
    case SYMLINK_ALWAYS_DEREF:  /* -L */
      options.xstat = optionl_stat;
      options.no_leaf_check = true;
      break;
      
    case SYMLINK_NEVER_DEREF:	/* -P (default) */
      options.xstat = optionp_stat;
      /* Can't turn no_leaf_check off because the user might have specified 
       * -noleaf anyway
       */
      break;
      
    case SYMLINK_DEREF_ARGSONLY: /* -H */
      options.xstat = optionh_stat;
      options.no_leaf_check = true;
    }

  options.symlink_handling = opt;
  
  /* For DBEUG_STAT, the choice is made at runtime within debug_stat()
   * by checking the contents of the symlink_handling variable.
   */
#if defined(DEBUG_STAT)
  options.xstat = debug_stat;
#endif /* !DEBUG_STAT */
}

/* Get the stat information for a file, if it is 
 * not already known. 
 */
int
get_statinfo (const char *pathname, const char *name, struct stat *p)
{
  if (!state.have_stat && (*options.xstat) (name, p) != 0)
    {
      if (!options.ignore_readdir_race || (errno != ENOENT) )
	{
	  error (0, errno, "%s", pathname);
	  state.exit_status = 1;
	}
      return -1;
    }
  state.have_stat = true;
  state.have_type = true;
  state.type = p->st_mode;
  return 0;
}


/* Get the stat/type information for a file, if it is 
 * not already known. 
 */
int
get_info (const char *pathname,
	  const char *name,
	  struct stat *p,
	  struct predicate *pred_ptr)
{
  /* If we need the full stat info, or we need the type info but don't 
   * already have it, stat the file now.
   */
  if (pred_ptr->need_stat)
    {
      return get_statinfo(pathname, state.rel_pathname, p);
    }
  if ((pred_ptr->need_type && (0 == state.have_type)))
    {
      return get_statinfo(pathname, state.rel_pathname, p);
    }
  return 0;
}

int
main (int argc, char **argv)
{
  int i;
  PFB parse_function;		/* Pointer to the function which parses. */
  struct predicate *cur_pred;
  char *predicate_name;		/* Name of predicate being parsed. */
  int end_of_leading_options = 0; /* First arg after any -H/-L etc. */
  program_name = argv[0];

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  atexit (close_stdout);
  
  if (isatty(0))
    {
      options.warnings = true;
    }
  else
    {
      options.warnings = false;
    }
  
  
  predicates = NULL;
  last_pred = NULL;
  options.do_dir_first = true;
  options.maxdepth = options.mindepth = -1;
  options.start_time = time (NULL);
  options.cur_day_start = options.start_time - DAYSECS;
  options.full_days = false;
  options.stay_on_filesystem = false;
  options.ignore_readdir_race = false;

  state.exit_status = 0;

#if defined(DEBUG_STAT)
  options.xstat = debug_stat;
#endif /* !DEBUG_STAT */

  if (getenv("POSIXLY_CORRECT"))
    options.output_block_size = 512;
  else
    options.output_block_size = 1024;
  
  if (getenv("FIND_BLOCK_SIZE"))
    {
      error (1, 0, _("The environment variable FIND_BLOCK_SIZE is not supported, the only thing that affects the block size is the POSIXLY_CORRECT environment variable"));
    }

  options.no_leaf_check = false;
  set_follow_state(SYMLINK_NEVER_DEREF); /* The default is equivalent to -P. */

  init_mounted_dev_list();

#ifdef DEBUG
  fprintf (stderr, "cur_day_start = %s", ctime (&options.cur_day_start));
#endif /* DEBUG */

  /* Check for -P, -H or -L options. */
  for (i=1; (end_of_leading_options = i) < argc; ++i)
    {
      if (0 == strcmp("-H", argv[i]))
	{
	  /* Meaning: dereference symbolic links on command line, but nowhere else. */
	  set_follow_state(SYMLINK_DEREF_ARGSONLY);
	}
      else if (0 == strcmp("-L", argv[i]))
	{
	  /* Meaning: dereference all symbolic links. */
	  set_follow_state(SYMLINK_ALWAYS_DEREF);
	}
      else if (0 == strcmp("-P", argv[i]))
	{
	  /* Meaning: never dereference symbolic links (default). */
	  set_follow_state(SYMLINK_NEVER_DEREF);
	}
      else if (0 == strcmp("--", argv[i]))
	{
	  /* -- signifies the end of options. */
	  end_of_leading_options = i+1;	/* Next time start with the next option */
	  break;
	}
      else
	{
	  /* Hmm, must be one of 
	   * (a) A path name
	   * (b) A predicate
	   */
	  end_of_leading_options = i; /* Next time start with this option */
	  break;
	}
    }

  /* We are now processing the part of the "find" command line 
   * after the -H/-L options (if any).
   */

  /* fprintf(stderr, "rest: optind=%ld\n", (long)optind); */
  
  /* Find where in ARGV the predicates begin. */
  for (i = end_of_leading_options; i < argc && strchr ("-!(),", argv[i][0]) == NULL; i++)
    {
      /* fprintf(stderr, "Looks like %s is not a predicate\n", argv[i]); */
      /* Do nothing. */ ;
    }
  
  /* Enclose the expression in `( ... )' so a default -print will
     apply to the whole expression. */
  parse_open (argv, &argc);
  /* Build the input order list. */
  while (i < argc)
    {
      if (strchr ("-!(),", argv[i][0]) == NULL)
	usage (_("paths must precede expression"));
      predicate_name = argv[i];
      parse_function = find_parser (predicate_name);
      if (parse_function == NULL)
	/* Command line option not recognized */
	error (1, 0, _("invalid predicate `%s'"), predicate_name);
      i++;
      if (!(*parse_function) (argv, &i))
	{
	  if (argv[i] == NULL)
	    /* Command line option requires an argument */
	    error (1, 0, _("missing argument to `%s'"), predicate_name);
	  else
	    error (1, 0, _("invalid argument `%s' to `%s'"),
		   argv[i], predicate_name);
	}
    }
  if (predicates->pred_next == NULL)
    {
      /* No predicates that do something other than set a global variable
	 were given; remove the unneeded initial `(' and add `-print'. */
      cur_pred = predicates;
      predicates = last_pred = predicates->pred_next;
      free ((char *) cur_pred);
      parse_print (argv, &argc);
    }
  else if (!default_prints (predicates->pred_next))
    {
      /* One or more predicates that produce output were given;
	 remove the unneeded initial `('. */
      cur_pred = predicates;
      predicates = predicates->pred_next;
      free ((char *) cur_pred);
    }
  else
    {
      /* `( user-supplied-expression ) -print'. */
      parse_close (argv, &argc);
      parse_print (argv, &argc);
    }

#ifdef	DEBUG
  fprintf (stderr, _("Predicate List:\n"));
  print_list (stderr, predicates);
#endif /* DEBUG */

  /* Done parsing the predicates.  Build the evaluation tree. */
  cur_pred = predicates;
  eval_tree = get_expr (&cur_pred, NO_PREC);

  /* Check if we have any left-over predicates (this fixes
   * Debian bug #185202).
   */
  if (cur_pred != NULL)
    {
      error (1, 0, _("unexpected extra predicate"));
    }
  
#ifdef	DEBUG
  fprintf (stderr, _("Eval Tree:\n"));
  print_tree (stderr, eval_tree, 0);
#endif /* DEBUG */

  /* Rearrange the eval tree in optimal-predicate order. */
  opt_expr (&eval_tree);

  /* Determine the point, if any, at which to stat the file. */
  mark_stat (eval_tree);
  /* Determine the point, if any, at which to determine file type. */
  mark_type (eval_tree);

#ifdef DEBUG
  fprintf (stderr, _("Optimized Eval Tree:\n"));
  print_tree (stderr, eval_tree, 0);
  fprintf (stderr, _("Optimized command line:\n"));
  print_optlist(stderr, eval_tree);
  fprintf(stderr, "\n");
#endif /* DEBUG */

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
  if ((*options.xstat) (".", &starting_stat_buf) != 0)
    error (1, errno, _("cannot get current directory"));

  /* If no paths are given, default to ".".  */
  for (i = end_of_leading_options; i < argc && strchr ("-!(),", argv[i][0]) == NULL; i++)
    {
      process_top_path (argv[i], 0);
    }

  /* If there were no path arguments, default to ".". */
  if (i == end_of_leading_options)
    {
      /* 
       * We use a temporary variable here because some actions modify 
       * the path temporarily.  Hence if we use a string constant, 
       * we get a coredump.  The best example of this is if we say 
       * "find -printf %H" (note, not "find . -printf %H").
       */
      char defaultpath[2] = ".";
      process_top_path (defaultpath, 0);
    }

  /* If "-exec ... {} +" has been used, there may be some 
   * partially-full command lines which have been built, 
   * but which are not yet complete.   Execute those now.
   */
  complete_pending_execs(eval_tree);
  
  return state.exit_status;
}


static char *
specific_dirname(const char *dir)
{
  char dirname[1024];

  if (0 == strcmp(".", dir))
    {
      /* OK, what's '.'? */
      if (NULL != getcwd(dirname, sizeof(dirname)))
	{
	  return strdup(dirname);
	}
      else
	{
	  return strdup(dir);
	}
    }
  else
    {
      char *result = canonicalize_filename_mode(dir, CAN_EXISTING);
      if (NULL == result)
	return strdup(dir);
      else
	return result;
    }
}

static dev_t *mounted_devices = NULL;
static size_t num_mounted_devices = 0u;


static void
init_mounted_dev_list()
{
  assert(NULL == mounted_devices);
  assert(0 == num_mounted_devices);
  mounted_devices = get_mounted_devices(&num_mounted_devices);
}

static void
refresh_mounted_dev_list(void)
{
  if (mounted_devices)
    {
      free(mounted_devices);
      mounted_devices = 0;
    }
  num_mounted_devices = 0u;
  init_mounted_dev_list();
}


/* Search for device DEV in the array LIST, which is of size N. */
static int
dev_present(dev_t dev, const dev_t *list, size_t n)
{
  if (list)
    {
      while (n-- > 0u)
	{
	  if ( (*list++) == dev )
	    return 1;
	}
    }
  return 0;
}

enum MountPointStateChange
  {
    MountPointRecentlyMounted,
    MountPointRecentlyUnmounted,
    MountPointStateUnchanged
  };



static enum MountPointStateChange
get_mount_state(dev_t newdev)
{
  int new_is_present, new_was_present;
  
  new_was_present = dev_present(newdev, mounted_devices, num_mounted_devices);
  refresh_mounted_dev_list();
  new_is_present  = dev_present(newdev, mounted_devices, num_mounted_devices);
  
  if (new_was_present == new_is_present)
    return MountPointStateUnchanged;
  else if (new_is_present)
    return MountPointRecentlyMounted;
  else
    return MountPointRecentlyUnmounted;
}


/* Return non-zero if FS is the name of a filesystem that is likely to
 * be automounted
 */
static int
fs_likely_to_be_automounted(const char *fs)
{
  return ( (0==strcmp(fs, "nfs")) || (0==strcmp(fs, "autofs")));
}

enum WdSanityCheckFatality
  {
    FATAL_IF_SANITY_CHECK_FAILS,
    NON_FATAL_IF_SANITY_CHECK_FAILS
  };


/* Examine the results of the stat() of a directory from before we
 * entered or left it, with the results of stat()ing it afterward.  If
 * these are different, the filesystem tree has been modified while we
 * were traversing it.  That might be an attempt to use a race
 * condition to persuade find to do something it didn't intend
 * (e.g. an attempt by an ordinary user to exploit the fact that root
 * sometimes runs find on the whole filesystem).  However, this can
 * also happen if automount is running (certainly on Solaris).  With 
 * automount, moving into a directory can cause a filesystem to be 
 * mounted there.
 *
 * To cope sensibly with this, we will raise an error if we see the
 * device number change unless we are chdir()ing into a subdirectory,
 * and the directory we moved into has been mounted or unmounted "recently".  
 * Here "recently" means since we started "find" or we last re-read 
 * the /etc/mnttab file. 
 *
 * If the device number does not change but the inode does, that is a
 * problem.
 *
 * If the device number and inode are both the same, we are happy.
 *
 * If a filesystem is (un)mounted as we chdir() into the directory, that 
 * may mean that we're now examining a section of the filesystem that might 
 * have been excluded from consideration (via -prune or -quit for example).
 * Hence we print a warning message to indicate that the output of find 
 * might be inconsistent due to the change in the filesystem.
 */
static boolean
wd_sanity_check(const char *thing_to_stat,
		const char *program_name,
		const char *what,
		dev_t old_dev,
		ino_t old_ino,
		struct stat *newinfo,
		int parent,
		int line_no,
		enum TraversalDirection direction,
		enum WdSanityCheckFatality isfatal,
		boolean *changed) /* output parameter */
{
  const char *fstype;
  char *specific_what = NULL;
  int silent = 0;
  
  *changed = false;
  
  if ((*options.xstat) (".", newinfo) != 0)
    error (1, errno, "%s", thing_to_stat);
  
  if (old_dev != newinfo->st_dev)
    {
      *changed = true;
      specific_what = specific_dirname(what);
      fstype = filesystem_type(newinfo);
      silent = fs_likely_to_be_automounted(fstype);
      
      /* This condition is rare, so once we are here it is 
       * reasonable to perform an expensive computation to 
       * determine if we should continue or fail. 
       */
      if (TraversingDown == direction)
	{
	  /* We stat()ed a directory, chdir()ed into it (we know this 
	   * since direction is TraversingDown), stat()ed it again,
	   * and noticed that the device numbers are different.  Check
	   * if the filesystem was recently mounted. 
	   * 
	   * If it was, it looks like chdir()ing into the directory
	   * caused a filesystem to be mounted.  Maybe automount is
	   * running.  Anyway, that's probably OK - but it happens
	   * only when we are moving downward.
	   *
	   * We also allow for the possibility that a similar thing
	   * has happened with the unmounting of a filesystem.  This
	   * is much rarer, as it relies on an automounter timeout
	   * occurring at exactly the wrong moment.
	   */
	  enum MountPointStateChange transition = get_mount_state(newinfo->st_dev);
	  switch (transition)
	    {
	    case MountPointRecentlyUnmounted:
	      isfatal = NON_FATAL_IF_SANITY_CHECK_FAILS;
	      if (!silent)
		{
		  error (0, 0,
			 _("Warning: filesystem %s has recently been unmounted."),
			 specific_what);
		}
	      break;
	      
	    case MountPointRecentlyMounted:
	      isfatal = NON_FATAL_IF_SANITY_CHECK_FAILS;
	      if (!silent)
		{
		  error (0, 0,
			 _("Warning: filesystem %s has recently been mounted."),
			 specific_what);
		}
	      break;

	    case MountPointStateUnchanged:
	      /* leave isfatal as it is */
	      break;
	    }
	}

      if (FATAL_IF_SANITY_CHECK_FAILS == isfatal)
	{
	  fstype = filesystem_type(newinfo);
	  error (1, 0,
		 _("%s%s changed during execution of %s (old device number %ld, new device number %ld, filesystem type is %s) [ref %ld]"),
		 specific_what,
		 parent ? "/.." : "",
		 program_name,
		 (long) old_dev,
		 (long) newinfo->st_dev,
		 fstype,
		 line_no);
	  /*NOTREACHED*/
	  return false;
	}
      else
	{
	  /* Since the device has changed under us, the inode number 
	   * will almost certainly also be different. However, we have 
	   * already decided that this is not a problem.  Hence we return
	   * without checking the inode number.
	   */
	  free(specific_what);
	  return true;
	}
    }

  /* Device number was the same, check if the inode has changed. */
  if (old_ino != newinfo->st_ino)
    {
      *changed = true;
      specific_what = specific_dirname(what);
      fstype = filesystem_type(newinfo);
      
      error ((isfatal == FATAL_IF_SANITY_CHECK_FAILS) ? 1 : 0,
	     0,			/* no relevant errno value */
	     _("%s%s changed during execution of %s (old inode number %ld, new inode number %ld, filesystem type is %s) [ref %ld]"),
	     specific_what, 
	     parent ? "/.." : "",
	     program_name,
	     (long) old_ino,
	     (long) newinfo->st_ino,
	     fstype,
	     line_no);
      free(specific_what);
      return false;
    }
  
  return true;
}

enum SafeChdirStatus
  {
    SafeChdirOK,
    SafeChdirFailSymlink,
    SafeChdirFailNotDir,
    SafeChdirFailStat,
    SafeChdirFailWouldBeUnableToReturn,
    SafeChdirFailChdirFailed,
    SafeChdirFailNonexistent
  };

/* Safely perform a change in directory.
 *
 */
static int 
safely_chdir(const char *dest,
	     enum TraversalDirection direction,
	     struct stat *statbuf_dest)
{
  struct stat statbuf_arrived;
  int rv, dotfd=-1;
  int saved_errno;		/* specific_dirname() changes errno. */
  char *name = NULL;
  boolean rv_set = false;
  
  saved_errno = errno = 0;
  dotfd = open(".", O_RDONLY);
  if (dotfd >= 0)
    {
      /* Stat the directory we're going to. */
      if (0 == options.xstat(dest, statbuf_dest))
	{
#ifdef S_ISLNK
	  if (!following_links() && S_ISLNK(statbuf_dest->st_mode))
	    {
	      rv = SafeChdirFailSymlink;
	      rv_set = true;
	      saved_errno = 0;	/* silence the error message */
	      goto fail;
	    }
#endif	  
#ifdef S_ISDIR
	  /* Although the immediately following chdir() would detect
	   * the fact that this is not a directory for us, this would
	   * result in an extra system call that fails.  Anybody
	   * examining the system-call trace should ideally not be
	   * concerned that something is actually failing.
	   */
	  if (!S_ISDIR(statbuf_dest->st_mode))
	    {
	      rv = SafeChdirFailNotDir;
	      rv_set = true;
	      saved_errno = 0;	/* silence the error message */
	      goto fail;
	    }
#endif	  
	  if (0 == chdir(dest))
	    {
	      /* check we ended up where we wanted to go */
	      boolean changed = false;
	      wd_sanity_check(".", program_name, ".",
			      statbuf_dest->st_dev,
			      statbuf_dest->st_ino,
			      &statbuf_arrived, 
			      0, __LINE__, direction,
			      FATAL_IF_SANITY_CHECK_FAILS,
			      &changed);
	      close(dotfd);
	      return SafeChdirOK;
	    }
	  else
	    {
	      saved_errno = errno;
	      if (ENOENT == saved_errno)
		{
		  rv = SafeChdirFailNonexistent;
		  rv_set = true;
		  if (options.ignore_readdir_race)
		    errno = 0;	/* don't issue err msg */
		  else
		    name = specific_dirname(dest);
		}
	      else if (ENOTDIR == saved_errno)
		{
		  /* This can happen if the we stat a directory,
		   * and then filesystem activity changes it into 
		   * a non-directory.
		   */
		  saved_errno = 0;	/* don't issue err msg */
		  rv = SafeChdirFailNotDir;
		  rv_set = true;
		}
	      else
		{
		  rv = SafeChdirFailChdirFailed;
		  rv_set = true;
		  name = specific_dirname(dest);
		}
	      goto fail;
	    }
	}
      else
	{
	  saved_errno = errno;
	  rv = SafeChdirFailStat;
	  rv_set = true;
	  name = specific_dirname(dest);
	  if ( (ENOENT == saved_errno) || (0 == state.curdepth))
	    saved_errno = 0;	/* don't issue err msg */
	  goto fail;
	}
    }
  else
    {
      /* We do not have read permissions on "." */
      rv = SafeChdirFailWouldBeUnableToReturn;
      rv_set = true;
      goto fail;
    }

  saved_errno = 0;
  
  /* We use the same exit path for successs or failure. 
   * which has occurred is recorded in RV. 
   */
 fail:
  if (saved_errno)
    {
      errno = saved_errno;
      
      /* do not call error() as this would result in a duplicate error message 
       * when the caller does the same thing. 
       */
    }
  
  free(name);
  name = NULL;
  
  if (dotfd >= 0)
    {
      close(dotfd);
      dotfd = -1;
    }
  assert(rv_set);
  return rv;
}


/* Safely go back to the starting directory. */
static void
chdir_back (void)
{
  struct stat stat_buf;
  boolean dummy;
  
  if (starting_desc < 0)
    {
      if (chdir (starting_dir) != 0)
	error (1, errno, "%s", starting_dir);

      wd_sanity_check(starting_dir,
		      program_name,
		      starting_dir,
		      starting_stat_buf.st_dev,
		      starting_stat_buf.st_ino,
		      &stat_buf, 0, __LINE__,
		      TraversingUp,
		      FATAL_IF_SANITY_CHECK_FAILS,
		      &dummy);
    }
  else
    {
      if (fchdir (starting_desc) != 0)
	error (1, errno, "%s", starting_dir);
    }
}

/* Descend PATHNAME, which is a command-line argument.  */
static void
process_top_path (char *pathname, mode_t mode)
{
  state.curdepth = 0;
  state.path_length = strlen (pathname);
  process_path (pathname, pathname, false, ".", mode);
}


/* Info on each directory in the current tree branch, to avoid
   getting stuck in symbolic link loops.  */
static struct dir_id *dir_ids = NULL;
/* Entries allocated in `dir_ids'.  */
static int dir_alloc = 0;
/* Index in `dir_ids' of directory currently being searched.
   This is always the last valid entry.  */
static int dir_curr = -1;
/* (Arbitrary) number of entries to grow `dir_ids' by.  */
#define DIR_ALLOC_STEP 32



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
issue_loop_warning(const char *name, const char *pathname, int level)
{
  struct stat stbuf_link;
  if (lstat(name, &stbuf_link) != 0)
    stbuf_link.st_mode = S_IFREG;
  
  if (S_ISLNK(stbuf_link.st_mode))
    {
      error(0, 0,
	    _("Symbolic link `%s' is part of a loop in the directory hierarchy; we have already visited the directory to which it points."),
	    pathname);
    }
  else
    {
      int distance = 1 + (dir_curr-level);
      /* We have found an infinite loop.  POSIX requires us to
       * issue a diagnostic.  Usually we won't get to here
       * because when the leaf optimisation is on, it will cause
       * the subdirectory to be skipped.  If /a/b/c/d is a hard
       * link to /a/b, then the link count of /a/b/c is 2,
       * because the ".." entry of /b/b/c/d points to /a, not
       * to /a/b/c.
       */
      error(0, 0,
	    _("Filesystem loop detected; `%s' has the same device number and inode as a directory which is %d %s."),
	    pathname,
	    distance,
	    (distance == 1 ?
	     _("level higher in the filesystem hierarchy") :
	     _("levels higher in the filesystem hierarchy")));
    }
}

/* Recursively descend path PATHNAME, applying the predicates.
   LEAF is true if PATHNAME is known to be in a directory that has no
   more unexamined subdirectories, and therefore it is not a directory.
   Knowing this allows us to avoid calling stat as long as possible for
   leaf files.

   NAME is PATHNAME relative to the current directory.  We access NAME
   but print PATHNAME.

   PARENT is the path of the parent of NAME, relative to find's
   starting directory.

   Return nonzero iff PATHNAME is a directory. */

static int
process_path (char *pathname, char *name, boolean leaf, char *parent,
	      mode_t mode)
{
  struct stat stat_buf;
  static dev_t root_dev;	/* Device ID of current argument pathname. */
  int i;

  /* Assume it is a non-directory initially. */
  stat_buf.st_mode = 0;
  state.rel_pathname = name;
  state.type = 0;

  /* If we know the type of the directory entry, and it is not a
   * symbolic link, we may be able to avoid a stat() or lstat() call.
   */
  if (mode)
    {
      if (S_ISLNK(mode) && following_links())
	{
	  /* mode is wrong because we should have followed the symlink. */
	  if (get_statinfo(pathname, name, &stat_buf) != 0)
	    return 0;
	  mode = state.type = stat_buf.st_mode;
	  state.have_type = true;
	}
      else
	{
	  state.have_type = true;
	  state.have_stat = false;
	  stat_buf.st_mode = mode;
	}
    }
  else
    {
      /* Mode is not yet known; may have to stat the file unless we 
       * can deduce that it is not a directory (which is all we need to 
       * know at this stage)
       */
      if (leaf)
	{
	  state.have_stat = false;
	  state.have_type = false;;
	  state.type = 0;
	}
      else
	{
	  state.have_stat = false;
	  if (get_statinfo(pathname, name, &stat_buf) != 0)
	    return 0;

	  /* If -L is in effect and we are dealing with a symlink,
	   * st_mode is the mode of the pointed-to file, while mode is
	   * the mode of the directory entry (S_IFLNK).  Hence now
	   * that we have the stat information, override "mode".
	   */
	  state.type = stat_buf.st_mode;
	  state.have_type = true;
	}
    }
  
  if (!S_ISDIR (state.type))
    {
      if (state.curdepth >= options.mindepth)
	apply_predicate (pathname, &stat_buf, eval_tree);
      return 0;
    }

  /* From here on, we're working on a directory.  */

  
  /* Now we really need to stat the directory, even if we knoe the
   * type, because we need information like struct stat.st_rdev.
   */
  if (get_statinfo(pathname, name, &stat_buf) != 0)
    return 0;

  state.have_stat = true;
  mode = state.type = stat_buf.st_mode;	/* use full info now that we have it. */
  state.stop_at_current_level =
    options.maxdepth >= 0
    && state.curdepth >= options.maxdepth;

  /* If we've already seen this directory on this branch,
     don't descend it again.  */
  for (i = 0; i <= dir_curr; i++)
    if (stat_buf.st_ino == dir_ids[i].ino &&
	stat_buf.st_dev == dir_ids[i].dev)
      {
	state.stop_at_current_level = true;
	issue_loop_warning(name, pathname, i);
      }
  
  if (dir_alloc <= ++dir_curr)
    {
      dir_alloc += DIR_ALLOC_STEP;
      dir_ids = (struct dir_id *)
	xrealloc ((char *) dir_ids, dir_alloc * sizeof (struct dir_id));
    }
  dir_ids[dir_curr].ino = stat_buf.st_ino;
  dir_ids[dir_curr].dev = stat_buf.st_dev;

  if (options.stay_on_filesystem)
    {
      if (state.curdepth == 0)
	root_dev = stat_buf.st_dev;
      else if (stat_buf.st_dev != root_dev)
	state.stop_at_current_level = true;
    }

  if (options.do_dir_first && state.curdepth >= options.mindepth)
    apply_predicate (pathname, &stat_buf, eval_tree);

#ifdef DEBUG
  fprintf(stderr, "pathname = %s, stop_at_current_level = %d\n",
	  pathname, state.stop_at_current_level);
#endif /* DEBUG */
  
  if (state.stop_at_current_level == false)
    /* Scan directory on disk. */
    process_dir (pathname, name, strlen (pathname), &stat_buf, parent);

  if (options.do_dir_first == false && state.curdepth >= options.mindepth)
    {
      state.rel_pathname = name;
      apply_predicate (pathname, &stat_buf, eval_tree);
    }

  dir_curr--;

  return 1;
}

/* Examine the predicate list for instances of -execdir or -okdir
 * which have been terminated with '+' (build argument list) rather
 * than ';' (singles only).  If there are any, run them (this will
 * have no effect if there are no arguments waiting).
 */
static void
complete_pending_execdirs(struct predicate *p)
{
#if defined(NEW_EXEC)
  if (NULL == p)
    return;
  
  complete_pending_execdirs(p->pred_left);
  
  if (p->pred_func == pred_execdir || p->pred_func == pred_okdir)
    {
      /* It's an exec-family predicate.  p->args.exec_val is valid. */
      if (p->args.exec_vec.multiple)
	{
	  struct exec_val *execp = &p->args.exec_vec;
	  
	  /* This one was terminated by '+' and so might have some
	   * left... Run it if neccessary.  
	   */
	  if (execp->state.todo)
	    {
	      /* There are not-yet-executed arguments. */
	      launch (&execp->ctl, &execp->state);
	    }
	}
    }

  complete_pending_execdirs(p->pred_right);
#else
  /* nothing to do. */
  return;
#endif
}

/* Examine the predicate list for instances of -exec which have been
 * terminated with '+' (build argument list) rather than ';' (singles
 * only).  If there are any, run them (this will have no effect if
 * there are no arguments waiting).
 */
static void
complete_pending_execs(struct predicate *p)
{
#if defined(NEW_EXEC)
  if (NULL == p)
    return;
  
  complete_pending_execs(p->pred_left);
  
  /* It's an exec-family predicate then p->args.exec_val is valid
   * and we can check it. 
   */
  if (p->pred_func == pred_exec && p->args.exec_vec.multiple)
    {
      struct exec_val *execp = &p->args.exec_vec;
      
      /* This one was terminated by '+' and so might have some
       * left... Run it if neccessary.  Set state.exit_status if
       * there are any problems.
       */
      if (execp->state.todo)
	{
	  /* There are not-yet-executed arguments. */
	  launch (&execp->ctl, &execp->state);
	}
    }

  complete_pending_execs(p->pred_right);
#else
  /* nothing to do. */
  return;
#endif
}


/* Scan directory PATHNAME and recurse through process_path for each entry.

   PATHLEN is the length of PATHNAME.

   NAME is PATHNAME relative to the current directory.

   STATP is the results of *options.xstat on it.

   PARENT is the path of the parent of NAME, relative to find's
   starting directory.  */

static void
process_dir (char *pathname, char *name, int pathlen, struct stat *statp, char *parent)
{
  char *name_space;		/* Names of files in PATHNAME. */
  int subdirs_left;		/* Number of unexamined subdirs in PATHNAME. */
  int idx;			/* Which entry are we on? */
  struct stat stat_buf;
  struct savedir_dirinfo *dirinfo;
  
  subdirs_left = statp->st_nlink - 2; /* Account for name and ".". */

  errno = 0;
  name_space = savedirinfo (name, &dirinfo);
  
  if (name_space == NULL)
    {
      assert(errno != 0);
      error (0, errno, "%s", pathname);
      state.exit_status = 1;
    }
  else
    {
      register char *namep;	/* Current point in `name_space'. */
      char *cur_path;		/* Full path of each file to process. */
      char *cur_name;		/* Base name of each file to process. */
      unsigned cur_path_size;	/* Bytes allocated for `cur_path'. */
      register unsigned file_len; /* Length of each path to process. */
      register unsigned pathname_len; /* PATHLEN plus trailing '/'. */

      if (pathname[pathlen - 1] == '/')
	pathname_len = pathlen + 1; /* For '\0'; already have '/'. */
      else
	pathname_len = pathlen + 2; /* For '/' and '\0'. */
      cur_path_size = 0;
      cur_path = NULL;

#if USE_SAFE_CHDIR
      if (strcmp (name, "."))
	{
	  enum SafeChdirStatus status = safely_chdir (name, TraversingDown, &stat_buf);
	  switch (status)
	    {
	    case SafeChdirOK:
	      /* If there had been a change but wd_sanity_check()
	       * accepted it, we need to accept that on the 
	       * way back up as well, so modify our record 
	       * of what we think we should see later.
	       * If there was no change, the assignments are a no-op.
	       */
	      dir_ids[dir_curr].dev = stat_buf.st_dev;
	      dir_ids[dir_curr].ino = stat_buf.st_ino;
	      break;
      
	    case SafeChdirFailNonexistent:
	    case SafeChdirFailStat:
	    case SafeChdirFailWouldBeUnableToReturn:
	    case SafeChdirFailSymlink:
	    case SafeChdirFailNotDir:
	    case SafeChdirFailChdirFailed:
	      error (0, errno, "%s", pathname);
	      state.exit_status = 1;
	      return;
	    }
	}
#else
      if (strcmp (name, ".") && chdir (name) < 0)
	{
	  error (0, errno, "%s", pathname);
	  exit_status = 1;
	  return;
	}

      /* Check that we are where we should be. */
      if (1)
	{
	  boolean changed = false;
	  wd_sanity_check(pathname,
			  program_name,
			  ".",
			  dir_ids[dir_curr].dev,
			  dir_ids[dir_curr].ino,
			  &stat_buf, 0, __LINE__, 
			  TraversingDown,
			  FATAL_IF_SANITY_CHECK_FAILS,
			  &changed);
	  if (changed)
	    {
	      /* If there had been a change but wd_sanity_check()
	       * accepted it, we need to accept that on the 
	       * way back up as well, so modify our record 
	       * of what we think we should see later. 
	       */
	      dir_ids[dir_curr].dev = stat_buf.st_dev;
	      dir_ids[dir_curr].ino = stat_buf.st_ino;
	    }
	}
#endif

      for (idx=0, namep = name_space; *namep; namep += file_len - pathname_len + 1, ++idx)
	{
	  /* savedirinfo() may return dirinfo=NULL if extended information 
	   * is not available. 
	   */
	  mode_t mode = dirinfo ? dirinfo[idx].type_info : 0;

	  /* Append this directory entry's name to the path being searched. */
	  file_len = pathname_len + strlen (namep);
	  if (file_len > cur_path_size)
	    {
	      while (file_len > cur_path_size)
		cur_path_size += 1024;
	      if (cur_path)
		free (cur_path);
	      cur_path = xmalloc (cur_path_size);
	      strcpy (cur_path, pathname);
	      cur_path[pathname_len - 2] = '/';
	    }
	  cur_name = cur_path + pathname_len - 1;
	  strcpy (cur_name, namep);

	  state.curdepth++;
	  if (!options.no_leaf_check)
	    {
	      if (mode && S_ISDIR(mode) && (subdirs_left == 0))
		{
		  /* This is a subdirectory, but the number of directories we 
		   * have found now exceeds the number we would expect given 
		   * the hard link count on the parent.   This is likely to be 
		   * a bug in the filesystem driver (e.g. Linux's 
		   * /proc filesystem) or may just be a fact that the OS 
		   * doesn't really handle hard links with Unix semantics.
		   * In the latter case, -noleaf should be used routinely.
		   */
		  error(0, 0, _("WARNING: Hard link count is wrong for %s: this may be a bug in your filesystem driver.  Automatically turning on find's -noleaf option.  Earlier results may have failed to include directories that should have been searched."),
			parent);
		  state.exit_status = 1; /* We know the result is wrong, now */
		  options.no_leaf_check = true;	/* Don't make same
						   mistake again */
		  subdirs_left = 1; /* band-aid for this iteration. */
		}
	      
	      /* Normal case optimization.  On normal Unix
		 filesystems, a directory that has no subdirectories
		 has two links: its name, and ".".  Any additional
		 links are to the ".." entries of its subdirectories.
		 Once we have processed as many subdirectories as
		 there are additional links, we know that the rest of
		 the entries are non-directories -- in other words,
		 leaf files. */
	      subdirs_left -= process_path (cur_path, cur_name,
					    subdirs_left == 0, pathname,
					    mode);
	    }
	  else
	    {
	      /* There might be weird (e.g., CD-ROM or MS-DOS) filesystems
		 mounted, which don't have Unix-like directory link counts. */
	      process_path (cur_path, cur_name, false, pathname, mode);
	    }
	  
	  state.curdepth--;
	}


      /* We're about to leave the directory.  If there are any
       * -execdir argument lists which have been built but have not
       * yet been processed, do them now because they must be done in
       * the same directory.
       */
      complete_pending_execdirs(eval_tree);

#if USE_SAFE_CHDIR
      if (strcmp (name, "."))
	{
	  enum SafeChdirStatus status;
	  struct dir_id did;
	  boolean changed = false;
	  
	  /* We could go back and do the next command-line arg
	     instead, maybe using longjmp.  */
	  char const *dir;
	  boolean deref = following_links() ? true : false;
	  
	  if ( (state.curdepth>0) && !deref)
	    dir = "..";
	  else
	    {
	      chdir_back ();
	      dir = parent;
	    }
	  
	  status = safely_chdir (dir, TraversingUp, &stat_buf);
	  switch (status)
	    {
	    case SafeChdirOK:
	      break;
      
	    case SafeChdirFailNonexistent:
	    case SafeChdirFailStat:
	    case SafeChdirFailWouldBeUnableToReturn:
	    case SafeChdirFailSymlink:
	    case SafeChdirFailNotDir:
	    case SafeChdirFailChdirFailed:
	      error (1, errno, "%s", pathname);
	      return;
	    }

	  if (dir_curr > 0)
	    {
	      did.dev = dir_ids[dir_curr-1].dev;
	      did.ino = dir_ids[dir_curr-1].ino;
	    }
	  else
	    {
	      did.dev = starting_stat_buf.st_dev;
	      did.ino = starting_stat_buf.st_ino;
	    }

	  wd_sanity_check(pathname,
			  program_name,
			  parent,
			  did.dev,
			  did.ino,
			  &stat_buf,
			  deref ? 1 : 0,
			  __LINE__, 
			  TraversingUp,
			  FATAL_IF_SANITY_CHECK_FAILS,
			  &changed);
	}
#else
      if (strcmp (name, "."))
	{
	  /* We could go back and do the next command-line arg
	     instead, maybe using longjmp.  */
	  char const *dir;
	  boolean deref = following_links() ? true : false;
	  
	  if (!deref)
	    dir = "..";
	  else
	    {
	      chdir_back ();
	      dir = parent;
	    }

	  if (chdir (dir) != 0)
	    error (1, errno, "%s", parent);

	  /* Check that we are where we should be. */
	  if (1)
	    {
	      boolean changed = false;
	      struct stat tmp;
	      int problem_is_with_parent;
	      
	      memset(&tmp, 0, sizeof(tmp));
	      if (dir_curr > 0)
		{
		  tmp.st_dev = dir_ids[dir_curr-1].dev;
		  tmp.st_ino = dir_ids[dir_curr-1].ino;
		}
	      else
		{
		  tmp.st_dev = starting_stat_buf.st_dev;
		  tmp.st_ino = starting_stat_buf.st_ino;
		}

	      problem_is_with_parent = deref ? 1 : 0;
	      wd_sanity_check(pathname,
			      program_name,
			      parent,
			      tmp.st_dev,
			      tmp.st_ino,
			      &stat_buf,
			      problem_is_with_parent, __LINE__, 
			      TraversingUp,
			      FATAL_IF_SANITY_CHECK_FAILS,
			      &changed);
	    }
	}
#endif

      if (cur_path)
	free (cur_path);
      free (name_space);
      free (dirinfo);
    }
}

/* Return true if there are no predicates with no_default_print in
   predicate list PRED, false if there are any.
   Returns true if default print should be performed */

static boolean
default_prints (struct predicate *pred)
{
  while (pred != NULL)
    {
      if (pred->no_default_print)
	return (false);
      pred = pred->pred_next;
    }
  return (true);
}
