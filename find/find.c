/* find -- search for files in a directory hierarchy
   Copyright (C) 1990, 91, 92, 93, 94, 2000, 2003 Free Software Foundation, Inc.

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
#include <modetype.h>
#include "../gnulib/lib/savedir.h"

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
static void process_top_path PARAMS((char *pathname));
static int process_path PARAMS((char *pathname, char *name, boolean leaf, char *parent));
static void process_dir PARAMS((char *pathname, char *name, int pathlen, struct stat *statp, char *parent));
#if 0
static boolean no_side_effects PARAMS((struct predicate *pred));
#endif
static boolean default_prints PARAMS((struct predicate *pred));

/* Name this program was run with. */
char *program_name;

/* All predicates for each path to process. */
struct predicate *predicates;

/* The last predicate allocated. */
struct predicate *last_pred;

/* The root of the evaluation tree. */
static struct predicate *eval_tree;

/* If true, process directory before contents.  True unless -depth given. */
boolean do_dir_first;

/* If >=0, don't descend more than this many levels of subdirectories. */
int maxdepth;

/* If >=0, don't process files above this level. */
int mindepth;

/* Current depth; 0 means current path is a command line arg. */
int curdepth;

/* Output block size.  */
int output_block_size;

/* Time at start of execution.  */
time_t start_time;

/* Seconds between 00:00 1/1/70 and either one day before now
   (the default), or the start of today (if -daystart is given). */
time_t cur_day_start;

/* If true, cur_day_start has been adjusted to the start of the day. */
boolean full_days;

/* If true, do not assume that files in directories with nlink == 2
   are non-directories. */
boolean no_leaf_check;

/* If true, don't cross filesystem boundaries. */
boolean stay_on_filesystem;

/* If true, don't descend past current directory.
   Can be set by -prune, -maxdepth, and -xdev/-mount. */
boolean stop_at_current_level;

/* The full path of the initial working directory, or "." if
   STARTING_DESC is nonnegative.  */
char const *starting_dir = ".";

/* A file descriptor open to the initial working directory.
   Doing it this way allows us to work when the i.w.d. has
   unreadable parents.  */
int starting_desc;

/* The stat buffer of the initial working directory. */
struct stat starting_stat_buf;

/* If true, we have called stat on the current path. */
boolean have_stat;

/* The file being operated on, relative to the current directory.
   Used for stat, readlink, remove, and opendir.  */
char *rel_pathname;

/* Length of current path. */
int path_length;

/* true if following symlinks.  Should be consistent with xstat.  */
/* boolean dereference; */
enum SymlinkOption symlink_handling;


/* Pointer to the function used to stat files. */
int (*xstat) ();

/* Status value to return to system. */
int exit_status;

/* If true, we ignore the problem where we find that a directory entry 
 * no longer exists by the time we get around to processing it.
 */
boolean ignore_readdir_race;


/* If true, we issue warning messages
 */
boolean warnings;


enum TraversalDirection
  {
    TraversingUp,
    TraversingDown
  };


static int
following_links(void)
{
  switch (symlink_handling)
    {
    case SYMLINK_ALWAYS_DEREF:
      return 1;
    case SYMLINK_DEREF_ARGSONLY:
      return (curdepth == 0);
    case SYMLINK_NEVER_DEREF:
    default:
      return 0;
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
  if (0 == curdepth) 
    {
      /* This file is from the command line; deference the link (if it
       * is a link).  
       */
      if (0 == stat(name, p))
	{
	  /* success */
	  return 0;
	}
      else
	{
	  /* fallback - return the information for the link itself. */
	  return lstat(name, p);
	}
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
  if (0 == stat(name, p))
    {
      return 0;			/* normal case. */
    }
  else
    {
      return lstat(name, p);	/* can't follow link, return the link itself. */
    }
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
static int
debug_stat (const char *file, struct stat *bufp)
{
  fprintf (stderr, "debug_stat (%s)\n", file);
  switch (symlink_handling)
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
      xstat = optionl_stat;
      no_leaf_check = false;
      break;
      
    case SYMLINK_NEVER_DEREF:	/* -P (default) */
      xstat = optionp_stat;
      /* Can't turn on no_leaf_check because the user might have specified 
       * -noleaf anyway
       */
      break;
      
    case SYMLINK_DEREF_ARGSONLY: /* -H */
      xstat = optionh_stat;
      no_leaf_check = true;
    }

  symlink_handling = opt;
  
  /* For DBEUG_STAT, the choice is made at runtime within debug_stat()
   * by checking the contents of the symlink_handling variable.
   */
#if defined(DEBUG_STAT)
  xstat = debug_stat;
#endif /* !DEBUG_STAT */
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

  if (isatty(0))
    {
      warnings = true;
    }
  else
    {
      warnings = false;
    }
  
  
  predicates = NULL;
  last_pred = NULL;
  do_dir_first = true;
  maxdepth = mindepth = -1;
  start_time = time (NULL);
  cur_day_start = start_time - DAYSECS;
  full_days = false;
  no_leaf_check = false;
  stay_on_filesystem = false;
  ignore_readdir_race = false;
  exit_status = 0;

#if defined(DEBUG_STAT)
  xstat = debug_stat;
#endif /* !DEBUG_STAT */

#if 0  
  human_block_size (getenv ("FIND_BLOCK_SIZE"), 0, &output_block_size);
#else
  if (getenv("POSIXLY_CORRECT"))
    output_block_size = 512;
  else
    output_block_size = 1024;
  
  if (getenv("FIND_BLOCK_SIZE"))
    {
      error (1, 0, _("The environment variable FIND_BLOCK_SIZE is not supported, the only thing that affects the block size is the POSIXLY_CORRECT environment variable"));
    }

  set_follow_state(SYMLINK_NEVER_DEREF); /* The default is equivalent to -P. */

  init_mounted_dev_list();

#endif

#ifdef DEBUG
  printf ("cur_day_start = %s", ctime (&cur_day_start));
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
  printf (_("Predicate List:\n"));
  print_list (predicates);
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
  printf (_("Eval Tree:\n"));
  print_tree (eval_tree, 0);
#endif /* DEBUG */

  /* Rearrange the eval tree in optimal-predicate order. */
  opt_expr (&eval_tree);

  /* Determine the point, if any, at which to stat the file. */
  mark_stat (eval_tree);

#ifdef DEBUG
  printf (_("Optimized Eval Tree:\n"));
  print_tree (eval_tree, 0);
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
  if ((*xstat) (".", &starting_stat_buf) != 0)
    error (1, errno, _("cannot get current directory"));

  /* If no paths are given, default to ".".  */
  for (i = end_of_leading_options; i < argc && strchr ("-!(),", argv[i][0]) == NULL; i++)
    {
      process_top_path (argv[i]);
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
      process_top_path (defaultpath);
    }
  

  return exit_status;
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
      const char *result = canonicalize_filename_mode(dir, CAN_EXISTING);
      if (NULL == result)
	return strdup(dir);
      else
	return result;
    }
}




#if 0
/* list_item_present: Search for NEEDLE in HAYSTACK.
 *
 * NEEDLE is a normal C string.  HAYSTACK is a list of concatenated C strings, 
 * each with a terminating NUL.   The last item in the list is identified by 
 * the fact that its terminating NUL is itself followed by a second NUL.
 *
 * This data structure does not lend itself to fast searching, but we only
 * do this when wd_sanity_check() thinks that a filesystem might have been 
 * mounted or unmounted.   That doesn't happen very often.
 */
static int
list_item_present(const char *needle, const char *haystack)
{
  if (NULL != haystack)
    {
      const char *s = haystack;
      while (*s)
	{
	  if (0 == strcmp(s, needle))
	    {
	      return 1;
	    }
	  else
	    {
	      s += strlen(s);
	      ++s;			/* skip the first NUL. */
	    }
	}
    }
  return 0;
}

static char *mount_points = NULL;


/* Initialise our idea of what the list of mount points is. 
 * this function is called exactly once.
 */
static void
init_mount_point_list(void)
{
  assert(NULL == mount_points);
  mount_points = get_mounted_filesystems();
}

static void
refresh_mount_point_list(void)
{
  if (mount_points)
    {
      free(mount_points);
      mount_points = NULL;
    }
  init_mount_point_list();
}

/* Determine if a directory has recently had a filesystem 
 * mounted on it or unmounted from it.
 */
static enum MountPointStateChange
get_mount_point_state(const char *dir)
{
  int was_mounted, is_mounted;

  was_mounted = list_item_present(dir, mount_points);
  refresh_mount_point_list();
  is_mounted = list_item_present(dir, mount_points);

  if (was_mounted == is_mounted)
    return MountPointStateUnchanged;
  else if (is_mounted)
    return MountPointRecentlyMounted;
  else 
    return MountPointRecentlyUnmounted;
}
#endif



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
  
  if ((*xstat) (".", newinfo) != 0)
    error (1, errno, "%s", thing_to_stat);
  
  if (old_dev != newinfo->st_dev)
    {
      *changed = true;
      specific_what = specific_dirname(what);
      fstype = filesystem_type(thing_to_stat, ".", newinfo);
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
	  fstype = filesystem_type(thing_to_stat, ".", newinfo);
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
      fstype = filesystem_type(thing_to_stat, ".", newinfo);
      
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
safely_chdir(const char *dest, enum TraversalDirection direction)
{
  struct stat statbuf_dest, statbuf_arrived;
  int rv, dotfd=-1;
  int saved_errno;		/* specific_dirname() changes errno. */
  char *name = NULL;
  boolean rv_set = false;
  
  saved_errno = errno = 0;
  dotfd = open(".", O_RDONLY);
  if (dotfd >= 0)
    {
      /* Stat the directory we're going to. */
      if (0 == xstat(dest, &statbuf_dest))
	{
#ifdef S_ISLNK
	  if (!following_links() && S_ISLNK(statbuf_dest.st_mode))
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
	  if (!S_ISDIR(statbuf_dest.st_mode))
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
			      statbuf_dest.st_dev,
			      statbuf_dest.st_ino,
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
		  if (ignore_readdir_race)
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
	  if ( (ENOENT == saved_errno) || (0 == curdepth))
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
      if (NULL == name)
	name = specific_dirname(".");
      error(0, saved_errno, "%s", name);
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
process_top_path (char *pathname)
{
  struct stat stat_buf, cur_stat_buf;
  boolean dummy;
  
  curdepth = 0;
  path_length = strlen (pathname);

  /* We stat each pathname given on the command-line twice --
     once here and once in process_path.  It's not too bad, though,
     since the kernel can read the stat information out of its inode
     cache the second time.  */
#if 0
  if ((*xstat) (pathname, &stat_buf) == 0 && S_ISDIR (stat_buf.st_mode))
    {
      if (chdir (pathname) < 0)
	{
	  if (!ignore_readdir_race || (errno != ENOENT) )
	    {
	      error (0, errno, "%s", pathname);
	      exit_status = 1;
	    }
	  return;
	}

      /* Check that we are where we should be. */
      wd_sanity_check(pathname, program_name,
		      ".",
		      stat_buf.st_dev,
		      stat_buf.st_ino,
		      &cur_stat_buf, 0, __LINE__,
		      TraversingDown,
		      FATAL_IF_SANITY_CHECK_FAILS,
		      &dummy);

      process_path (pathname, ".", false, ".");
      chdir_back ();
    }
  else
    {
      process_path (pathname, pathname, false, ".");
    }
#else
  enum SafeChdirStatus rv = safely_chdir(pathname, TraversingDown);
  
  switch (rv)
    {
    case SafeChdirOK:
      process_path (pathname, ".", false, ".");
      chdir_back ();
      return;
      
    case SafeChdirFailNonexistent:
    case SafeChdirFailStat:
    case SafeChdirFailWouldBeUnableToReturn:
    case SafeChdirFailSymlink:
    case SafeChdirFailNotDir:
    case SafeChdirFailChdirFailed:
      if ((SafeChdirFailNonexistent==rv) && !ignore_readdir_race)
	{
	  error (0, errno, "%s", pathname);
	  exit_status = 1;
	}
      else
	{
	  process_path (pathname, pathname, false, ".");
	}
      chdir_back ();
      return;
    }
#endif
}

/* Info on each directory in the current tree branch, to avoid
   getting stuck in symbolic link loops.  */
struct dir_id
{
  ino_t ino;
  dev_t dev;
};
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
  
  if (!S_ISLNK(stbuf_link.st_mode))
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
process_path (char *pathname, char *name, boolean leaf, char *parent)
{
  struct stat stat_buf;
  static dev_t root_dev;	/* Device ID of current argument pathname. */
  int i;

  /* Assume it is a non-directory initially. */
  stat_buf.st_mode = 0;

  rel_pathname = name;

  if (leaf)
    have_stat = false;
  else
    {
      if ((*xstat) (name, &stat_buf) != 0)
	{
	  if (!ignore_readdir_race || (errno != ENOENT) )
	    {
	      error (0, errno, "%s", pathname);
	      exit_status = 1;
	    }
	  return 0;
	}
      have_stat = true;
    }

  if (!S_ISDIR (stat_buf.st_mode))
    {
      if (curdepth >= mindepth)
	apply_predicate (pathname, &stat_buf, eval_tree);
      return 0;
    }

  /* From here on, we're working on a directory.  */

  stop_at_current_level = maxdepth >= 0 && curdepth >= maxdepth;

  /* If we've already seen this directory on this branch,
     don't descend it again.  */
  for (i = 0; i <= dir_curr; i++)
    if (stat_buf.st_ino == dir_ids[i].ino &&
	stat_buf.st_dev == dir_ids[i].dev)
      {
	stop_at_current_level = true;
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

  if (stay_on_filesystem)
    {
      if (curdepth == 0)
	root_dev = stat_buf.st_dev;
      else if (stat_buf.st_dev != root_dev)
	stop_at_current_level = true;
    }

  if (do_dir_first && curdepth >= mindepth)
    apply_predicate (pathname, &stat_buf, eval_tree);

#ifdef DEBUG
  fprintf(stderr, "pathname = %s, stop_at_current_level = %d\n",
	  pathname, stop_at_current_level);
#endif /* DEBUG */
  
  if (stop_at_current_level == false)
    /* Scan directory on disk. */
    process_dir (pathname, name, strlen (pathname), &stat_buf, parent);

  if (do_dir_first == false && curdepth >= mindepth)
    {
      rel_pathname = name;
      apply_predicate (pathname, &stat_buf, eval_tree);
    }

  dir_curr--;

  return 1;
}

/* Scan directory PATHNAME and recurse through process_path for each entry.

   PATHLEN is the length of PATHNAME.

   NAME is PATHNAME relative to the current directory.

   STATP is the results of *xstat on it.

   PARENT is the path of the parent of NAME, relative to find's
   starting directory.  */

static void
process_dir (char *pathname, char *name, int pathlen, struct stat *statp, char *parent)
{
  char *name_space;		/* Names of files in PATHNAME. */
  int subdirs_left;		/* Number of unexamined subdirs in PATHNAME. */
  struct stat stat_buf;

  subdirs_left = statp->st_nlink - 2; /* Account for name and ".". */

  errno = 0;
  name_space = savedir (name);
  if (name_space == NULL)
    {
      if (errno)
	{
	  error (0, errno, "%s", pathname);
	  exit_status = 1;
	}
      else
	error (1, 0, _("virtual memory exhausted"));
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

      for (namep = name_space; *namep; namep += file_len - pathname_len + 1)
	{
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

	  curdepth++;
	  if (!no_leaf_check)
	    /* Normal case optimization.
	       On normal Unix filesystems, a directory that has no
	       subdirectories has two links: its name, and ".".  Any
	       additional links are to the ".." entries of its
	       subdirectories.  Once we have processed as many
	       subdirectories as there are additional links, we know
	       that the rest of the entries are non-directories --
	       in other words, leaf files. */
	    subdirs_left -= process_path (cur_path, cur_name,
					  subdirs_left == 0, pathname);
	  else
	    /* There might be weird (e.g., CD-ROM or MS-DOS) filesystems
	       mounted, which don't have Unix-like directory link counts. */
	    process_path (cur_path, cur_name, false, pathname);
	  curdepth--;
	}

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

      if (cur_path)
	free (cur_path);
      free (name_space);
    }
}

#if 0
/* Return true if there are no side effects in any of the predicates in
   predicate list PRED, false if there are any. */

static boolean
no_side_effects (struct predicate *pred)
{
  while (pred != NULL)
    {
      if (pred->side_effects)
	return (false);
      pred = pred->pred_next;
    }
  return (true);
}
#endif

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
