/* parser.c -- convert the command line args into an expression tree.
   Copyright (C) 1990, 91, 92, 93, 94, 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 9 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.
*/

#include "defs.h"
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include "modechange.h"
#include "modetype.h"
#include "xstrtol.h"

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif
#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
# define N_(String) (String)
#endif

#if !defined (isascii) || defined (STDC_HEADERS)
#ifdef isascii
#undef isascii
#endif
#define isascii(c) 1
#endif

#define ISDIGIT(c) (isascii (c) && isdigit (c))
#define ISUPPER(c) (isascii (c) && isupper (c))

#ifndef HAVE_ENDGRENT
#define endgrent()
#endif
#ifndef HAVE_ENDPWENT
#define endpwent()
#endif

static boolean parse_amin PARAMS((char *argv[], int *arg_ptr));
static boolean parse_and PARAMS((char *argv[], int *arg_ptr));
static boolean parse_anewer PARAMS((char *argv[], int *arg_ptr));
static boolean parse_atime PARAMS((char *argv[], int *arg_ptr));
boolean parse_close PARAMS((char *argv[], int *arg_ptr));
static boolean parse_cmin PARAMS((char *argv[], int *arg_ptr));
static boolean parse_cnewer PARAMS((char *argv[], int *arg_ptr));
static boolean parse_comma PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ctime PARAMS((char *argv[], int *arg_ptr));
static boolean parse_daystart PARAMS((char *argv[], int *arg_ptr));
static boolean parse_depth PARAMS((char *argv[], int *arg_ptr));
static boolean parse_empty PARAMS((char *argv[], int *arg_ptr));
static boolean parse_exec PARAMS((char *argv[], int *arg_ptr));
static boolean parse_false PARAMS((char *argv[], int *arg_ptr));
static boolean parse_fls PARAMS((char *argv[], int *arg_ptr));
static boolean parse_fprintf PARAMS((char *argv[], int *arg_ptr));
static boolean parse_follow PARAMS((char *argv[], int *arg_ptr));
static boolean parse_fprint PARAMS((char *argv[], int *arg_ptr));
static boolean parse_fprint0 PARAMS((char *argv[], int *arg_ptr));
static boolean parse_fstype PARAMS((char *argv[], int *arg_ptr));
static boolean parse_gid PARAMS((char *argv[], int *arg_ptr));
static boolean parse_group PARAMS((char *argv[], int *arg_ptr));
static boolean parse_help PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ilname PARAMS((char *argv[], int *arg_ptr));
static boolean parse_iname PARAMS((char *argv[], int *arg_ptr));
static boolean parse_inum PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ipath PARAMS((char *argv[], int *arg_ptr));
static boolean parse_iregex PARAMS((char *argv[], int *arg_ptr));
static boolean parse_links PARAMS((char *argv[], int *arg_ptr));
static boolean parse_lname PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ls PARAMS((char *argv[], int *arg_ptr));
static boolean parse_maxdepth PARAMS((char *argv[], int *arg_ptr));
static boolean parse_mindepth PARAMS((char *argv[], int *arg_ptr));
static boolean parse_mmin PARAMS((char *argv[], int *arg_ptr));
static boolean parse_mtime PARAMS((char *argv[], int *arg_ptr));
static boolean parse_name PARAMS((char *argv[], int *arg_ptr));
static boolean parse_negate PARAMS((char *argv[], int *arg_ptr));
static boolean parse_newer PARAMS((char *argv[], int *arg_ptr));
static boolean parse_noleaf PARAMS((char *argv[], int *arg_ptr));
static boolean parse_nogroup PARAMS((char *argv[], int *arg_ptr));
static boolean parse_nouser PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ok PARAMS((char *argv[], int *arg_ptr));
boolean parse_open PARAMS((char *argv[], int *arg_ptr));
static boolean parse_or PARAMS((char *argv[], int *arg_ptr));
static boolean parse_path PARAMS((char *argv[], int *arg_ptr));
static boolean parse_perm PARAMS((char *argv[], int *arg_ptr));
boolean parse_print PARAMS((char *argv[], int *arg_ptr));
static boolean parse_print0 PARAMS((char *argv[], int *arg_ptr));
static boolean parse_printf PARAMS((char *argv[], int *arg_ptr));
static boolean parse_prune PARAMS((char *argv[], int *arg_ptr));
static boolean parse_regex PARAMS((char *argv[], int *arg_ptr));
static boolean insert_regex PARAMS((char *argv[], int *arg_ptr, boolean ignore_case));
static boolean parse_size PARAMS((char *argv[], int *arg_ptr));
static boolean parse_true PARAMS((char *argv[], int *arg_ptr));
static boolean parse_type PARAMS((char *argv[], int *arg_ptr));
static boolean parse_uid PARAMS((char *argv[], int *arg_ptr));
static boolean parse_used PARAMS((char *argv[], int *arg_ptr));
static boolean parse_user PARAMS((char *argv[], int *arg_ptr));
static boolean parse_version PARAMS((char *argv[], int *arg_ptr));
static boolean parse_xdev PARAMS((char *argv[], int *arg_ptr));
static boolean parse_xtype PARAMS((char *argv[], int *arg_ptr));

static boolean insert_regex PARAMS((char *argv[], int *arg_ptr, boolean ignore_case));
static boolean insert_type PARAMS((char *argv[], int *arg_ptr, boolean (*which_pred )()));
static boolean insert_fprintf PARAMS((FILE *fp, boolean (*func )(), char *argv[], int *arg_ptr));
static struct segment **make_segment PARAMS((struct segment **segment, char *format, int len, int kind));
static boolean insert_exec_ok PARAMS((boolean (*func )(), char *argv[], int *arg_ptr));
static boolean get_num_days PARAMS((char *str, uintmax_t *num_days, enum comparison_type *comp_type));
static boolean insert_time PARAMS((char *argv[], int *arg_ptr, PFB pred));
static boolean get_num PARAMS((char *str, uintmax_t *num, enum comparison_type *comp_type));
static boolean insert_num PARAMS((char *argv[], int *arg_ptr, PFB pred));
static FILE *open_output_file PARAMS((char *path));

#ifdef DEBUG
char *find_pred_name PARAMS((PFB pred_func));
#endif /* DEBUG */

struct parser_table
{
  char *parser_name;
  PFB parser_func;
};

/* GNU find predicates that are not mentioned in POSIX.2 are marked `GNU'.
   If they are in some Unix versions of find, they are marked `Unix'. */

static struct parser_table const parse_table[] =
{
  {"!", parse_negate},
  {"not", parse_negate},	/* GNU */
  {"(", parse_open},
  {")", parse_close},
  {",", parse_comma},		/* GNU */
  {"a", parse_and},
  {"amin", parse_amin},		/* GNU */
  {"and", parse_and},		/* GNU */
  {"anewer", parse_anewer},	/* GNU */
  {"atime", parse_atime},
  {"cmin", parse_cmin},		/* GNU */
  {"cnewer", parse_cnewer},	/* GNU */
#ifdef UNIMPLEMENTED_UNIX
  /* It's pretty ugly for find to know about archive formats.
     Plus what it could do with cpio archives is very limited.
     Better to leave it out.  */
  {"cpio", parse_cpio},		/* Unix */
#endif
  {"ctime", parse_ctime},
  {"daystart", parse_daystart},	/* GNU */
  {"depth", parse_depth},
  {"empty", parse_empty},	/* GNU */
  {"exec", parse_exec},
  {"false", parse_false},	/* GNU */
  {"fls", parse_fls},		/* GNU */
  {"follow", parse_follow},	/* GNU, Unix */
  {"fprint", parse_fprint},	/* GNU */
  {"fprint0", parse_fprint0},	/* GNU */
  {"fprintf", parse_fprintf},	/* GNU */
  {"fstype", parse_fstype},	/* GNU, Unix */
  {"gid", parse_gid},		/* GNU */
  {"group", parse_group},
  {"help", parse_help},		/* GNU */
  {"-help", parse_help},	/* GNU */
  {"ilname", parse_ilname},	/* GNU */
  {"iname", parse_iname},	/* GNU */
  {"inum", parse_inum},		/* GNU, Unix */
  {"ipath", parse_ipath},	/* GNU */
  {"iregex", parse_iregex},	/* GNU */
  {"links", parse_links},
  {"lname", parse_lname},	/* GNU */
  {"ls", parse_ls},		/* GNU, Unix */
  {"maxdepth", parse_maxdepth},	/* GNU */
  {"mindepth", parse_mindepth},	/* GNU */
  {"mmin", parse_mmin},		/* GNU */
  {"mount", parse_xdev},	/* Unix */
  {"mtime", parse_mtime},
  {"name", parse_name},
#ifdef UNIMPLEMENTED_UNIX
  {"ncpio", parse_ncpio},	/* Unix */
#endif
  {"newer", parse_newer},
  {"noleaf", parse_noleaf},	/* GNU */
  {"nogroup", parse_nogroup},
  {"nouser", parse_nouser},
  {"o", parse_or},
  {"or", parse_or},		/* GNU */
  {"ok", parse_ok},
  {"path", parse_path},		/* GNU, HP-UX */
  {"perm", parse_perm},
  {"print", parse_print},
  {"print0", parse_print0},	/* GNU */
  {"printf", parse_printf},	/* GNU */
  {"prune", parse_prune},
  {"regex", parse_regex},	/* GNU */
  {"size", parse_size},
  {"true", parse_true},		/* GNU */
  {"type", parse_type},
  {"uid", parse_uid},		/* GNU */
  {"used", parse_used},		/* GNU */
  {"user", parse_user},
  {"version", parse_version},	/* GNU */
  {"-version", parse_version},	/* GNU */
  {"xdev", parse_xdev},
  {"xtype", parse_xtype},	/* GNU */
  {0, 0}
};

/* Return a pointer to the parser function to invoke for predicate
   SEARCH_NAME.
   Return NULL if SEARCH_NAME is not a valid predicate name. */

PFB
find_parser (char *search_name)
{
  int i;

  if (*search_name == '-')
    search_name++;
  for (i = 0; parse_table[i].parser_name != 0; i++)
    if (strcmp (parse_table[i].parser_name, search_name) == 0)
      return (parse_table[i].parser_func);
  return (NULL);
}

/* The parsers are responsible to continue scanning ARGV for
   their arguments.  Each parser knows what is and isn't
   allowed for itself.
   
   ARGV is the argument array.
   *ARG_PTR is the index to start at in ARGV,
   updated to point beyond the last element consumed.
 
   The predicate structure is updated with the new information. */

static boolean
parse_amin (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  uintmax_t num;
  enum comparison_type c_type;
  time_t t;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (!get_num_days (argv[*arg_ptr], &num, &c_type))
    return (false);
  t = cur_day_start + DAYSECS - num * 60;
  our_pred = insert_primary (pred_amin);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.negative = t < 0;
  our_pred->args.info.l_val = t;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_and (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = get_new_pred ();
  our_pred->pred_func = pred_and;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_and);
#endif	/* DEBUG */
  our_pred->p_type = BI_OP;
  our_pred->p_prec = AND_PREC;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_anewer (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  struct stat stat_newer;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if ((*xstat) (argv[*arg_ptr], &stat_newer))
    error (1, errno, "%s", argv[*arg_ptr]);
  our_pred = insert_primary (pred_anewer);
  our_pred->args.time = stat_newer.st_mtime;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_atime (char **argv, int *arg_ptr)
{
  return (insert_time (argv, arg_ptr, pred_atime));
}

boolean
parse_close (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = get_new_pred ();
  our_pred->pred_func = pred_close;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_close);
#endif	/* DEBUG */
  our_pred->p_type = CLOSE_PAREN;
  our_pred->p_prec = NO_PREC;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_cmin (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  uintmax_t num;
  enum comparison_type c_type;
  time_t t;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (!get_num_days (argv[*arg_ptr], &num, &c_type))
    return (false);
  t = cur_day_start + DAYSECS - num * 60;
  our_pred = insert_primary (pred_cmin);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.negative = t < 0;
  our_pred->args.info.l_val = t;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_cnewer (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  struct stat stat_newer;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if ((*xstat) (argv[*arg_ptr], &stat_newer))
    error (1, errno, "%s", argv[*arg_ptr]);
  our_pred = insert_primary (pred_cnewer);
  our_pred->args.time = stat_newer.st_mtime;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_comma (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = get_new_pred ();
  our_pred->pred_func = pred_comma;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_comma);
#endif /* DEBUG */
  our_pred->p_type = BI_OP;
  our_pred->p_prec = COMMA_PREC;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_ctime (char **argv, int *arg_ptr)
{
  return (insert_time (argv, arg_ptr, pred_ctime));
}

static boolean
parse_daystart (char **argv, int *arg_ptr)
{
  struct tm *local;

  if (full_days == false)
    {
      cur_day_start += DAYSECS;
      local = localtime (&cur_day_start);
      cur_day_start -= (local
			? (local->tm_sec + local->tm_min * 60
			   + local->tm_hour * 3600)
			: cur_day_start % DAYSECS);
      full_days = true;
    }
  return (true);
}

static boolean
parse_depth (char **argv, int *arg_ptr)
{
  do_dir_first = false;
  return (true);
}
 
static boolean
parse_empty (char **argv, int *arg_ptr)
{
  insert_primary (pred_empty);
  return (true);
}

static boolean
parse_exec (char **argv, int *arg_ptr)
{
  return (insert_exec_ok (pred_exec, argv, arg_ptr));
}

static boolean
parse_false (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_false);
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_fls (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_fls);
  our_pred->args.stream = open_output_file (argv[*arg_ptr]);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  (*arg_ptr)++;
  return (true);
}

static boolean 
parse_fprintf (char **argv, int *arg_ptr)
{
  FILE *fp;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (argv[*arg_ptr + 1] == NULL)
    {
      /* Ensure we get "missing arg" message, not "invalid arg".  */
      (*arg_ptr)++;
      return (false);
    }
  fp = open_output_file (argv[*arg_ptr]);
  (*arg_ptr)++;
  return (insert_fprintf (fp, pred_fprintf, argv, arg_ptr));
}

static boolean
parse_follow (char **argv, int *arg_ptr)
{
  dereference = true;
  xstat = stat;
  no_leaf_check = true;
  return (true);
}

static boolean
parse_fprint (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_fprint);
  our_pred->args.stream = open_output_file (argv[*arg_ptr]);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->need_stat = false;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_fprint0 (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_fprint0);
  our_pred->args.stream = open_output_file (argv[*arg_ptr]);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->need_stat = false;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_fstype (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_fstype);
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_gid (char **argv, int *arg_ptr)
{
  return (insert_num (argv, arg_ptr, pred_gid));
}

static boolean
parse_group (char **argv, int *arg_ptr)
{
  struct group *cur_gr;
  struct predicate *our_pred;
  gid_t gid;
  int gid_len;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  cur_gr = getgrnam (argv[*arg_ptr]);
  endgrent ();
  if (cur_gr != NULL)
    gid = cur_gr->gr_gid;
  else
    {
      gid_len = strspn (argv[*arg_ptr], "0123456789");
      if ((gid_len == 0) || (argv[*arg_ptr][gid_len] != '\0'))
	return (false);
      gid = atoi (argv[*arg_ptr]);
    }
  our_pred = insert_primary (pred_group);
  our_pred->args.gid = gid;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_help (char **argv, int *arg_ptr)
{
  printf (_("\
Usage: %s [path...] [expression]\n"), program_name);
  puts (_("\
default path is the current directory; default expression is -print\n\
expression may consist of:\n\
operators (decreasing precedence; -and is implicit where no others are given):\n\
      ( EXPR ) ! EXPR -not EXPR EXPR1 -a EXPR2 EXPR1 -and EXPR2\n"));
  puts (_("\
      EXPR1 -o EXPR2 EXPR1 -or EXPR2 EXPR1 , EXPR2\n\
options (always true): -daystart -depth -follow --help\n\
      -maxdepth LEVELS -mindepth LEVELS -mount -noleaf --version -xdev\n\
tests (N can be +N or -N or N): -amin N -anewer FILE -atime N -cmin N\n"));
  puts (_("\
      -cnewer FILE -ctime N -empty -false -fstype TYPE -gid N -group NAME\n\
      -ilname PATTERN -iname PATTERN -inum N -ipath PATTERN -iregex PATTERN\n\
      -links N -lname PATTERN -mmin N -mtime N -name PATTERN -newer FILE\n"));
  puts (_("\
      -nouser -nogroup -path PATTERN -perm [+-]MODE -regex PATTERN\n\
      -size N[bckw] -true -type [bcdpfls] -uid N -used N -user NAME\n\
      -xtype [bcdpfls]\n"));
  puts (_("\
actions: -exec COMMAND ; -fprint FILE -fprint0 FILE -fprintf FILE FORMAT\n\
      -ok COMMAND ; -print -print0 -printf FORMAT -prune -ls\n"));
  puts (_("\nReport bugs to <bug-findutils@gnu.org>."));
  exit (0);
}

static boolean
parse_ilname (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_ilname);
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_iname (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_iname);
  our_pred->need_stat = false;
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_inum (char **argv, int *arg_ptr)
{
  return (insert_num (argv, arg_ptr, pred_inum));
}

static boolean
parse_ipath (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_ipath);
  our_pred->need_stat = false;
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_iregex (char **argv, int *arg_ptr)
{
  return insert_regex (argv, arg_ptr, true);
}

static boolean
parse_links (char **argv, int *arg_ptr)
{
  return (insert_num (argv, arg_ptr, pred_links));
}

static boolean
parse_lname (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_lname);
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_ls (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_ls);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  return (true);
}

static boolean
parse_maxdepth (char **argv, int *arg_ptr)
{
  int depth_len;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  depth_len = strspn (argv[*arg_ptr], "0123456789");
  if ((depth_len == 0) || (argv[*arg_ptr][depth_len] != '\0'))
    return (false);
  maxdepth = atoi (argv[*arg_ptr]);
  if (maxdepth < 0)
    return (false);
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_mindepth (char **argv, int *arg_ptr)
{
  int depth_len;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  depth_len = strspn (argv[*arg_ptr], "0123456789");
  if ((depth_len == 0) || (argv[*arg_ptr][depth_len] != '\0'))
    return (false);
  mindepth = atoi (argv[*arg_ptr]);
  if (mindepth < 0)
    return (false);
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_mmin (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  uintmax_t num;
  enum comparison_type c_type;
  time_t t;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (!get_num_days (argv[*arg_ptr], &num, &c_type))
    return (false);
  t = cur_day_start + DAYSECS - num * 60;
  our_pred = insert_primary (pred_mmin);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.negative = t < 0;
  our_pred->args.info.l_val = t;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_mtime (char **argv, int *arg_ptr)
{
  return (insert_time (argv, arg_ptr, pred_mtime));
}

static boolean
parse_name (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_name);
  our_pred->need_stat = false;
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_negate (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = get_new_pred_chk_op ();
  our_pred->pred_func = pred_negate;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_negate);
#endif	/* DEBUG */
  our_pred->p_type = UNI_OP;
  our_pred->p_prec = NEGATE_PREC;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_newer (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  struct stat stat_newer;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if ((*xstat) (argv[*arg_ptr], &stat_newer))
    error (1, errno, "%s", argv[*arg_ptr]);
  our_pred = insert_primary (pred_newer);
  our_pred->args.time = stat_newer.st_mtime;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_noleaf (char **argv, int *arg_ptr)
{
  no_leaf_check = true;
  return true;
}

#ifdef CACHE_IDS
/* Arbitrary amount by which to increase size
   of `uid_unused' and `gid_unused'. */
#define ALLOC_STEP 2048

/* Boolean: if uid_unused[n] is nonzero, then UID n has no passwd entry. */
char *uid_unused = NULL;

/* Number of elements in `uid_unused'. */
unsigned uid_allocated;

/* Similar for GIDs and group entries. */
char *gid_unused = NULL;
unsigned gid_allocated;
#endif

static boolean
parse_nogroup (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_nogroup);
#ifdef CACHE_IDS
  if (gid_unused == NULL)
    {
      struct group *gr;

      gid_allocated = ALLOC_STEP;
      gid_unused = xmalloc (gid_allocated);
      memset (gid_unused, 1, gid_allocated);
      setgrent ();
      while ((gr = getgrent ()) != NULL)
	{
	  if ((unsigned) gr->gr_gid >= gid_allocated)
	    {
	      unsigned new_allocated = (unsigned) gr->gr_gid + ALLOC_STEP;
	      gid_unused = xrealloc (gid_unused, new_allocated);
	      memset (gid_unused + gid_allocated, 1,
		      new_allocated - gid_allocated);
	      gid_allocated = new_allocated;
	    }
	  gid_unused[(unsigned) gr->gr_gid] = 0;
	}
      endgrent ();
    }
#endif
  return (true);
}

static boolean
parse_nouser (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_nouser);
#ifdef CACHE_IDS
  if (uid_unused == NULL)
    {
      struct passwd *pw;

      uid_allocated = ALLOC_STEP;
      uid_unused = xmalloc (uid_allocated);
      memset (uid_unused, 1, uid_allocated);
      setpwent ();
      while ((pw = getpwent ()) != NULL)
	{
	  if ((unsigned) pw->pw_uid >= uid_allocated)
	    {
	      unsigned new_allocated = (unsigned) pw->pw_uid + ALLOC_STEP;
	      uid_unused = xrealloc (uid_unused, new_allocated);
	      memset (uid_unused + uid_allocated, 1,
		      new_allocated - uid_allocated);
	      uid_allocated = new_allocated;
	    }
	  uid_unused[(unsigned) pw->pw_uid] = 0;
	}
      endpwent ();
    }
#endif
  return (true);
}

static boolean
parse_ok (char **argv, int *arg_ptr)
{
  return (insert_exec_ok (pred_ok, argv, arg_ptr));
}

boolean
parse_open (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = get_new_pred_chk_op ();
  our_pred->pred_func = pred_open;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_open);
#endif	/* DEBUG */
  our_pred->p_type = OPEN_PAREN;
  our_pred->p_prec = NO_PREC;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_or (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = get_new_pred ();
  our_pred->pred_func = pred_or;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_or);
#endif	/* DEBUG */
  our_pred->p_type = BI_OP;
  our_pred->p_prec = OR_PREC;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_path (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_path);
  our_pred->need_stat = false;
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_perm (char **argv, int *arg_ptr)
{
  mode_t perm_val;
  int mode_start = 0;
  struct mode_change *change;
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);

  switch (argv[*arg_ptr][0])
    {
    case '-':
    case '+':
      mode_start = 1;
      break;
    default:
      /* empty */
      break;
    }

  change = mode_compile (argv[*arg_ptr] + mode_start, MODE_MASK_PLUS);
  if (change == MODE_INVALID)
    error (1, 0, _("invalid mode `%s'"), argv[*arg_ptr]);
  else if (change == MODE_MEMORY_EXHAUSTED)
    error (1, 0, _("virtual memory exhausted"));
  perm_val = mode_adjust (0, change);
  mode_free (change);

  our_pred = insert_primary (pred_perm);

  switch (argv[*arg_ptr][0])
    {
    case '-':
      our_pred->args.perm.kind = PERM_AT_LEAST;
      break;
    case '+':
      our_pred->args.perm.kind = PERM_ANY;
      break;
    default:
      our_pred->args.perm.kind = PERM_EXACT;
      break;
    }
  our_pred->args.perm.val = perm_val & MODE_ALL;
  (*arg_ptr)++;
  return (true);
}

boolean
parse_print (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_print);
  /* -print has the side effect of printing.  This prevents us
     from doing undesired multiple printing when the user has
     already specified -print. */
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_print0 (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_print0);
  /* -print0 has the side effect of printing.  This prevents us
     from doing undesired multiple printing when the user has
     already specified -print0. */
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_printf (char **argv, int *arg_ptr)
{
  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  return (insert_fprintf (stdout, pred_fprintf, argv, arg_ptr));
}

static boolean
parse_prune (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_prune);
  our_pred->need_stat = false;
  /* -prune has a side effect that it does not descend into
     the current directory. */
  our_pred->side_effects = true;
  return (true);
}

static boolean
parse_regex (char **argv, int *arg_ptr)
{
  return insert_regex (argv, arg_ptr, false);
}

static boolean
insert_regex (char **argv, int *arg_ptr, boolean ignore_case)
{
  struct predicate *our_pred;
  struct re_pattern_buffer *re;
  const char *error_message;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_regex);
  our_pred->need_stat = false;
  re = (struct re_pattern_buffer *)
    xmalloc (sizeof (struct re_pattern_buffer));
  our_pred->args.regex = re;
  re->allocated = 100;
  re->buffer = (unsigned char *) xmalloc (re->allocated);
  re->fastmap = NULL;

  if (ignore_case)
    {
      unsigned i;
      
      re->translate = xmalloc (256);
      /* Map uppercase characters to corresponding lowercase ones.  */
      for (i = 0; i < 256; i++)
        re->translate[i] = ISUPPER (i) ? tolower (i) : i;
    }
  else
    re->translate = NULL;

  error_message = re_compile_pattern (argv[*arg_ptr], strlen (argv[*arg_ptr]),
				      re);
  if (error_message)
    error (1, 0, "%s", error_message);
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_size (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  uintmax_t num;
  enum comparison_type c_type;
  int blksize = 512;
  int len;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  len = strlen (argv[*arg_ptr]);
  if (len == 0)
    error (1, 0, _("invalid null argument to -size"));
  switch (argv[*arg_ptr][len - 1])
    {
    case 'b':
      blksize = 512;
      argv[*arg_ptr][len - 1] = '\0';
      break;

    case 'c':
      blksize = 1;
      argv[*arg_ptr][len - 1] = '\0';
      break;

    case 'k':
      blksize = 1024;
      argv[*arg_ptr][len - 1] = '\0';
      break;

    case 'w':
      blksize = 2;
      argv[*arg_ptr][len - 1] = '\0';
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      break;

    default:
      error (1, 0, _("invalid -size type `%c'"), argv[*arg_ptr][len - 1]);
    }
  if (!get_num (argv[*arg_ptr], &num, &c_type))
    return (false);
  our_pred = insert_primary (pred_size);
  our_pred->args.size.kind = c_type;
  our_pred->args.size.blocksize = blksize;
  our_pred->args.size.size = num;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_true (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  our_pred = insert_primary (pred_true);
  our_pred->need_stat = false;
  return (true);
}

static boolean
parse_type (char **argv, int *arg_ptr)
{
  return insert_type (argv, arg_ptr, pred_type);
}

static boolean
parse_uid (char **argv, int *arg_ptr)
{
  return (insert_num (argv, arg_ptr, pred_uid));
}

static boolean
parse_used (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  uintmax_t num_days;
  enum comparison_type c_type;
  time_t t;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (!get_num (argv[*arg_ptr], &num_days, &c_type))
    return (false);
  t = num_days * DAYSECS;
  our_pred = insert_primary (pred_used);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.negative = t < 0;
  our_pred->args.info.l_val = t;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_user (char **argv, int *arg_ptr)
{
  struct passwd *cur_pwd;
  struct predicate *our_pred;
  uid_t uid;
  int uid_len;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  cur_pwd = getpwnam (argv[*arg_ptr]);
  endpwent ();
  if (cur_pwd != NULL)
    uid = cur_pwd->pw_uid;
  else
    {
      uid_len = strspn (argv[*arg_ptr], "0123456789");
      if ((uid_len == 0) || (argv[*arg_ptr][uid_len] != '\0'))
	return (false);
      uid = atoi (argv[*arg_ptr]);
    }
  our_pred = insert_primary (pred_user);
  our_pred->args.uid = uid;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_version (char **argv, int *arg_ptr)
{
  extern char *version_string;

  fflush (stderr);
  printf (_("GNU find version %s\n"), version_string);
  exit (0);
}

static boolean
parse_xdev (char **argv, int *arg_ptr)
{
  stay_on_filesystem = true;
  return true;
}

static boolean
parse_xtype (char **argv, int *arg_ptr)
{
  return insert_type (argv, arg_ptr, pred_xtype);
}

static boolean
insert_type (char **argv, int *arg_ptr, boolean (*which_pred) (/* ??? */))
{
  mode_t type_cell;
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL)
      || (strlen (argv[*arg_ptr]) != 1))
    return (false);
  switch (argv[*arg_ptr][0])
    {
    case 'b':			/* block special */
      type_cell = S_IFBLK;
      break;
    case 'c':			/* character special */
      type_cell = S_IFCHR;
      break;
    case 'd':			/* directory */
      type_cell = S_IFDIR;
      break;
    case 'f':			/* regular file */
      type_cell = S_IFREG;
      break;
#ifdef S_IFLNK
    case 'l':			/* symbolic link */
      type_cell = S_IFLNK;
      break;
#endif
#ifdef S_IFIFO
    case 'p':			/* pipe */
      type_cell = S_IFIFO;
      break;
#endif
#ifdef S_IFSOCK
    case 's':			/* socket */
      type_cell = S_IFSOCK;
      break;
#endif
#ifdef S_IFDOOR
    case 'D':			/* Solaris door */
      type_cell = S_IFDOOR;
      break;
#endif
    default:			/* None of the above ... nuke 'em. */
      return (false);
    }
  our_pred = insert_primary (which_pred);
  our_pred->args.type = type_cell;
  (*arg_ptr)++;			/* Move on to next argument. */
  return (true);
}

/* If true, we've determined that the current fprintf predicate
   uses stat information. */
static boolean fprintf_stat_needed;

static boolean
insert_fprintf (FILE *fp, boolean (*func) (/* ??? */), char **argv, int *arg_ptr)
{
  char *format;			/* Beginning of unprocessed format string. */
  register char *scan;		/* Current address in scanning `format'. */
  register char *scan2;		/* Address inside of element being scanned. */
  struct segment **segmentp;	/* Address of current segment. */
  struct predicate *our_pred;

  format = argv[(*arg_ptr)++];

  fprintf_stat_needed = false;	/* Might be overridden later. */
  our_pred = insert_primary (func);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->args.printf_vec.stream = fp;
  segmentp = &our_pred->args.printf_vec.segment;
  *segmentp = NULL;

  for (scan = format; *scan; scan++)
    {
      if (*scan == '\\')
	{
	  scan2 = scan + 1;
	  if (*scan2 >= '0' && *scan2 <= '7')
	    {
	      register int n, i;

	      for (i = n = 0; i < 3 && (*scan2 >= '0' && *scan2 <= '7');
		   i++, scan2++)
		n = 8 * n + *scan2 - '0';
	      scan2--;
	      *scan = n;
	    }
	  else
	    {
	      switch (*scan2)
		{
		case 'a':
		  *scan = 7;
		  break;
		case 'b':
		  *scan = '\b';
		  break;
		case 'c':
		  make_segment (segmentp, format, scan - format, KIND_STOP);
		  our_pred->need_stat = fprintf_stat_needed;
		  return (true);
		case 'f':
		  *scan = '\f';
		  break;
		case 'n':
		  *scan = '\n';
		  break;
		case 'r':
		  *scan = '\r';
		  break;
		case 't':
		  *scan = '\t';
		  break;
		case 'v':
		  *scan = '\v';
		  break;
		case '\\':
		  /* *scan = '\\'; * it already is */
		  break;
		default:
		  error (0, 0,
			 _("warning: unrecognized escape `\\%c'"), *scan2);
		  scan++;
		  continue;
		}
	    }
	  segmentp = make_segment (segmentp, format, scan - format + 1,
				   KIND_PLAIN);
	  format = scan2 + 1;	/* Move past the escape. */
	  scan = scan2;		/* Incremented immediately by `for'. */
	}
      else if (*scan == '%')
	{
	  if (scan[1] == '%')
	    {
	      segmentp = make_segment (segmentp, format, scan - format + 1,
				       KIND_PLAIN);
	      scan++;
	      format = scan + 1;
	      continue;
	    }
	  /* Scan past flags, width and precision, to verify kind. */
	  for (scan2 = scan; *++scan2 && strchr ("-+ #", *scan2);)
	    /* Do nothing. */ ;
	  while (ISDIGIT (*scan2))
	    scan2++;
	  if (*scan2 == '.')
	    for (scan2++; ISDIGIT (*scan2); scan2++)
	      /* Do nothing. */ ;
	  if (strchr ("abcdfFgGhHiklmnpPstuU", *scan2))
	    {
	      segmentp = make_segment (segmentp, format, scan2 - format,
				       (int) *scan2);
	      scan = scan2;
	      format = scan + 1;
	    }
	  else if (strchr ("ACT", *scan2) && scan2[1])
	    {
	      segmentp = make_segment (segmentp, format, scan2 - format,
				       *scan2 | (scan2[1] << 8));
	      scan = scan2 + 1;
	      format = scan + 1;
	      continue;
	    }
	  else
	    {
	      /* An unrecognized % escape.  Print the char after the %. */
	      error (0, 0, _("warning: unrecognized format directive `%%%c'"),
		     *scan2);
	      segmentp = make_segment (segmentp, format, scan - format,
				       KIND_PLAIN);
	      format = scan + 1;
	      continue;
	    }
	}
    }

  if (scan > format)
    make_segment (segmentp, format, scan - format, KIND_PLAIN);
  our_pred->need_stat = fprintf_stat_needed;
  return (true);
}

/* Create a new fprintf segment in *SEGMENT, with type KIND,
   from the text in FORMAT, which has length LEN.
   Return the address of the `next' pointer of the new segment. */

static struct segment **
make_segment (struct segment **segment, char *format, int len, int kind)
{
  char *fmt;

  *segment = (struct segment *) xmalloc (sizeof (struct segment));

  (*segment)->kind = kind;
  (*segment)->next = NULL;
  (*segment)->text_len = len;

  fmt = (*segment)->text = xmalloc (len + sizeof "d");
  strncpy (fmt, format, len);
  fmt += len;

  switch (kind & 0xff)
    {
    case KIND_PLAIN:		/* Plain text string, no % conversion. */
    case KIND_STOP:		/* Terminate argument, no newline. */
      break;

    case 'a':			/* atime in `ctime' format */
    case 'A':			/* atime in user-specified strftime format */
    case 'b':			/* size in 512-byte blocks */
    case 'c':			/* ctime in `ctime' format */
    case 'C':			/* ctime in user-specified strftime format */
    case 'F':			/* filesystem type */
    case 'G':			/* GID number */
    case 'g':			/* group name */
    case 'i':			/* inode number */
    case 'k':			/* size in 1K blocks */
    case 'l':			/* object of symlink */
    case 'n':			/* number of links */
    case 's':			/* size in bytes */
    case 't':			/* mtime in `ctime' format */
    case 'T':			/* mtime in user-specified strftime format */
    case 'U':			/* UID number */
    case 'u':			/* user name */
      fprintf_stat_needed = true;
      /* FALLTHROUGH */
    case 'f':			/* basename of path */
    case 'h':			/* leading directories part of path */
    case 'H':			/* ARGV element file was found under */
    case 'p':			/* pathname */
    case 'P':			/* pathname with ARGV element stripped */
      *fmt++ = 's';
      break;

    case 'd':			/* depth in search tree (0 = ARGV element) */
      *fmt++ = 'd';
      break;

    case 'm':			/* mode as octal number (perms only) */
      *fmt++ = 'o';
      fprintf_stat_needed = true;
      break;
    }
  *fmt = '\0';

  return (&(*segment)->next);
}

/* handles both exec and ok predicate */
static boolean
insert_exec_ok (boolean (*func) (/* ??? */), char **argv, int *arg_ptr)
{
  int start, end;		/* Indexes in ARGV of start & end of cmd. */
  int num_paths;		/* Number of args with path replacements. */
  int path_pos;			/* Index in array of path replacements. */
  int vec_pos;			/* Index in array of args. */
  struct predicate *our_pred;
  struct exec_val *execp;	/* Pointer for efficiency. */

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);

  /* Count the number of args with path replacements, up until the ';'. */
  start = *arg_ptr;
  for (end = start, num_paths = 0;
       (argv[end] != NULL)
       && ((argv[end][0] != ';') || (argv[end][1] != '\0'));
       end++)
    if (strstr (argv[end], "{}"))
      num_paths++;
  /* Fail if no command given or no semicolon found. */
  if ((end == start) || (argv[end] == NULL))
    {
      *arg_ptr = end;
      return (false);
    }

  our_pred = insert_primary (func);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  execp = &our_pred->args.exec_vec;
  execp->paths =
    (struct path_arg *) xmalloc (sizeof (struct path_arg) * (num_paths + 1));
  execp->vec = (char **) xmalloc (sizeof (char *) * (end - start + 1));
  /* Record the positions of all args, and the args with path replacements. */
  for (end = start, path_pos = vec_pos = 0;
       (argv[end] != NULL)
       && ((argv[end][0] != ';') || (argv[end][1] != '\0'));
       end++)
    {
      register char *p;
      
      execp->paths[path_pos].count = 0;
      for (p = argv[end]; *p; ++p)
	if (p[0] == '{' && p[1] == '}')
	  {
	    execp->paths[path_pos].count++;
	    ++p;
	  }
      if (execp->paths[path_pos].count)
	{
	  execp->paths[path_pos].offset = vec_pos;
	  execp->paths[path_pos].origarg = argv[end];
	  path_pos++;
	}
      execp->vec[vec_pos++] = argv[end];
    }
  execp->paths[path_pos].offset = -1;
  execp->vec[vec_pos] = NULL;

  if (argv[end] == NULL)
    *arg_ptr = end;
  else
    *arg_ptr = end + 1;
  return (true);
}

/* Get a number of days and comparison type.
   STR is the ASCII representation.
   Set *NUM_DAYS to the number of days, taken as being from
   the current moment (or possibly midnight).  Thus the sense of the
   comparison type appears to be reversed.
   Set *COMP_TYPE to the kind of comparison that is requested.

   Return true if all okay, false if input error.

   Used by -atime, -ctime and -mtime (parsers) to
   get the appropriate information for a time predicate processor. */

static boolean
get_num_days (char *str, uintmax_t *num_days, enum comparison_type *comp_type)
{
  boolean r = get_num (str, num_days, comp_type);
  if (r)
    switch (*comp_type)
      {
      case COMP_LT: *comp_type = COMP_GT; break;
      case COMP_GT: *comp_type = COMP_LT; break;
      default: break;
      }
  return r;
}

/* Insert a time predicate PRED.
   ARGV is a pointer to the argument array.
   ARG_PTR is a pointer to an index into the array, incremented if
   all went well.

   Return true if input is valid, false if not.

   A new predicate node is assigned, along with an argument node
   obtained with malloc.

   Used by -atime, -ctime, and -mtime parsers. */

static boolean
insert_time (char **argv, int *arg_ptr, PFB pred)
{
  struct predicate *our_pred;
  uintmax_t num_days;
  enum comparison_type c_type;
  time_t t;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (!get_num_days (argv[*arg_ptr], &num_days, &c_type))
    return (false);
  t = (cur_day_start - num_days * DAYSECS
       + ((c_type == COMP_GT) ? DAYSECS - 1 : 0));
  our_pred = insert_primary (pred);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.negative = t < 0;
  our_pred->args.info.l_val = t;
  (*arg_ptr)++;
#ifdef	DEBUG
  printf (_("inserting %s\n"), our_pred->p_name);
  printf (_("    type: %s    %s  "),
	  (c_type == COMP_GT) ? "gt" :
	  ((c_type == COMP_LT) ? "lt" : ((c_type == COMP_EQ) ? "eq" : "?")),
	  (c_type == COMP_GT) ? " >" :
	  ((c_type == COMP_LT) ? " <" : ((c_type == COMP_EQ) ? ">=" : " ?")));
  t = our_pred->args.info.l_val;
  printf ("%ju %s", (uintmax_t) our_pred->args.info.l_val, ctime (&t));
  if (c_type == COMP_EQ)
    {
      t = our_pred->args.info.l_val += DAYSECS;
      printf ("                 <  %ju %s",
	      (uintmax_t) our_pred->args.info.l_val, ctime (&t));
      our_pred->args.info.l_val -= DAYSECS;
    }
#endif	/* DEBUG */
  return (true);
}

/* Get a number with comparision information.
   The sense of the comparision information is 'normal'; that is,
   '+' looks for a count > than the number and '-' less than.
   
   STR is the ASCII representation of the number.
   Set *NUM to the number.
   Set *COMP_TYPE to the kind of comparison that is requested.
 
   Return true if all okay, false if input error.  */

static boolean
get_num (char *str, uintmax_t *num, enum comparison_type *comp_type)
{
  int len_num;			/* Length of field. */

  if (str == NULL)
    return (false);
  switch (str[0])
    {
    case '+':
      *comp_type = COMP_GT;
      str++;
      break;
    case '-':
      *comp_type = COMP_LT;
      str++;
      break;
    default:
      *comp_type = COMP_EQ;
      break;
    }

  return xstrtoumax (str, NULL, 10, num, "") == LONGINT_OK;
}

/* Insert a number predicate.
   ARGV is a pointer to the argument array.
   *ARG_PTR is an index into ARGV, incremented if all went well.
   *PRED is the predicate processor to insert.

   Return true if input is valid, false if error.
   
   A new predicate node is assigned, along with an argument node
   obtained with malloc.

   Used by -inum and -links parsers. */

static boolean
insert_num (char **argv, int *arg_ptr, PFB pred)
{
  struct predicate *our_pred;
  uintmax_t num;
  enum comparison_type c_type;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if (!get_num (argv[*arg_ptr], &num, &c_type))
    return (false);
  our_pred = insert_primary (pred);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.l_val = num;
  (*arg_ptr)++;
#ifdef	DEBUG
  printf (_("inserting %s\n"), our_pred->p_name);
  printf (_("    type: %s    %s  "),
	  (c_type == COMP_GT) ? "gt" :
	  ((c_type == COMP_LT) ? "lt" : ((c_type == COMP_EQ) ? "eq" : "?")),
	  (c_type == COMP_GT) ? " >" :
	  ((c_type == COMP_LT) ? " <" : ((c_type == COMP_EQ) ? " =" : " ?")));
  printf ("%ju\n", our_pred->args.info.l_val);
#endif	/* DEBUG */
  return (true);
}

static FILE *
open_output_file (char *path)
{
  FILE *f;

  if (!strcmp (path, "/dev/stderr"))
    return (stderr);
  else if (!strcmp (path, "/dev/stdout"))
    return (stdout);
  f = fopen (path, "w");
  if (f == NULL)
    error (1, errno, "%s", path);
  return (f);
}
