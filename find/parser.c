/* parser.c -- convert the command line args into an expression tree.
   Copyright (C) 1990, 91, 92, 93, 94, 2000, 2001, 2003, 2004 Free Software Foundation, Inc.

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
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <fnmatch.h>
#include "../gnulib/lib/modechange.h"
#include "modetype.h"
#include "../gnulib/lib/xstrtol.h"
#include "../gnulib/lib/xalloc.h"
#include "buildcmd.h"
#include "nextelem.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

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

#if !defined (isascii) || defined (STDC_HEADERS)
#ifdef isascii
#undef isascii
#endif
#define isascii(c) 1
#endif

#define ISDIGIT(c) (isascii ((unsigned char)c) && isdigit ((unsigned char)c))
#define ISUPPER(c) (isascii ((unsigned char)c) && isupper ((unsigned char)c))

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
static boolean parse_delete PARAMS((char *argv[], int *arg_ptr));
static boolean parse_d PARAMS((char *argv[], int *arg_ptr));
static boolean parse_depth PARAMS((char *argv[], int *arg_ptr));
static boolean parse_empty PARAMS((char *argv[], int *arg_ptr));
static boolean parse_exec PARAMS((char *argv[], int *arg_ptr));
static boolean parse_execdir PARAMS((char *argv[], int *arg_ptr));
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
static boolean parse_iwholename PARAMS((char *argv[], int *arg_ptr));
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
static boolean parse_nowarn PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ok PARAMS((char *argv[], int *arg_ptr));
static boolean parse_okdir PARAMS((char *argv[], int *arg_ptr));
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
static boolean parse_samefile PARAMS((char *argv[], int *arg_ptr));
static boolean parse_size PARAMS((char *argv[], int *arg_ptr));
static boolean parse_true PARAMS((char *argv[], int *arg_ptr));
static boolean parse_type PARAMS((char *argv[], int *arg_ptr));
static boolean parse_uid PARAMS((char *argv[], int *arg_ptr));
static boolean parse_used PARAMS((char *argv[], int *arg_ptr));
static boolean parse_user PARAMS((char *argv[], int *arg_ptr));
static boolean parse_version PARAMS((char *argv[], int *arg_ptr));
static boolean parse_wholename PARAMS((char *argv[], int *arg_ptr));
static boolean parse_xdev PARAMS((char *argv[], int *arg_ptr));
static boolean parse_ignore_race PARAMS((char *argv[], int *arg_ptr));
static boolean parse_noignore_race PARAMS((char *argv[], int *arg_ptr));
static boolean parse_warn PARAMS((char *argv[], int *arg_ptr));
static boolean parse_xtype PARAMS((char *argv[], int *arg_ptr));
static boolean parse_quit PARAMS((char *argv[], int *arg_ptr));

static boolean insert_regex PARAMS((char *argv[], int *arg_ptr, boolean ignore_case));
static boolean insert_type PARAMS((char *argv[], int *arg_ptr, boolean (*which_pred )()));
static boolean insert_fprintf PARAMS((FILE *fp, boolean (*func )(), char *argv[], int *arg_ptr));
static struct segment **make_segment PARAMS((struct segment **segment, char *format, int len, int kind));
static boolean insert_exec_ok PARAMS((const char *action, boolean (*func )(), char *argv[], int *arg_ptr));
static boolean get_num_days PARAMS((char *str, uintmax_t *num_days, enum comparison_type *comp_type));
static boolean insert_time PARAMS((char *argv[], int *arg_ptr, PFB pred));
static boolean get_num PARAMS((char *str, uintmax_t *num, enum comparison_type *comp_type));
static boolean insert_num PARAMS((char *argv[], int *arg_ptr, PFB pred));
static FILE *open_output_file PARAMS((char *path));

#ifdef DEBUG
char *find_pred_name PARAMS((PFB pred_func));
#endif /* DEBUG */



enum arg_type
  {
    ARG_OPTION,			/* regular options like -maxdepth */
    ARG_POSITIONAL_OPTION,	/* options whose position is important (-follow) */
    ARG_TEST,			/* a like -name */
    ARG_PUNCTUATION,		/* like -o or ( */
    ARG_ACTION			/* like -print */
  };


struct parser_table
{
  enum arg_type type;
  char *parser_name;
  PFB parser_func;
};

/* GNU find predicates that are not mentioned in POSIX.2 are marked `GNU'.
   If they are in some Unix versions of find, they are marked `Unix'. */

static struct parser_table const parse_table[] =
{
  {ARG_PUNCTUATION,        "!",                     parse_negate},
  {ARG_PUNCTUATION,        "not",                   parse_negate},	/* GNU */
  {ARG_PUNCTUATION,        "(",                     parse_open},
  {ARG_PUNCTUATION,        ")",                     parse_close},
  {ARG_PUNCTUATION,        ",",                     parse_comma},	/* GNU */
  {ARG_PUNCTUATION,        "a",                     parse_and},
  {ARG_TEST,               "amin",                  parse_amin},	/* GNU */
  {ARG_PUNCTUATION,        "and",                   parse_and},		/* GNU */
  {ARG_TEST,               "anewer",                parse_anewer},	/* GNU */
  {ARG_TEST,               "atime",                 parse_atime},
  {ARG_TEST,               "cmin",                  parse_cmin},	/* GNU */
  {ARG_TEST,               "cnewer",                parse_cnewer},	/* GNU */
#ifdef UNIMPLEMENTED_UNIX
  /* It's pretty ugly for find to know about archive formats.
     Plus what it could do with cpio archives is very limited.
     Better to leave it out.  */
  {ARG_UNIMPLEMENTED,      "cpio",                  parse_cpio},        /* Unix */
#endif						    
  {ARG_TEST,               "ctime",                 parse_ctime},
  {ARG_POSITIONAL_OPTION,  "daystart",              parse_daystart},	/* GNU */
  {ARG_ACTION,             "delete",                parse_delete},	/* GNU, Mac OS, FreeBSD */
  {ARG_OPTION,             "d",                     parse_d},		/* Mac OS X, FreeBSD, NetBSD, OpenBSD, but deprecated  in favour of -depth */
  {ARG_OPTION,             "depth",                 parse_depth},
  {ARG_TEST,               "empty",                 parse_empty},	/* GNU */
  {ARG_ACTION,             "exec",                  parse_exec},
  {ARG_ACTION,             "execdir",               parse_execdir},     /* *BSD, GNU */
  {ARG_TEST,               "false",                 parse_false},	/* GNU */
  {ARG_ACTION,             "fls",                   parse_fls},		/* GNU */
  {ARG_POSITIONAL_OPTION,  "follow",                parse_follow},	/* GNU, Unix */
  {ARG_ACTION,             "fprint",                parse_fprint},	/* GNU */
  {ARG_ACTION,             "fprint0",               parse_fprint0},	/* GNU */
  {ARG_ACTION,             "fprintf",               parse_fprintf},	/* GNU */
  {ARG_TEST,               "fstype",                parse_fstype},	/* GNU, Unix */
  {ARG_TEST,               "gid",                   parse_gid},		/* GNU */
  {ARG_TEST,               "group",                 parse_group},
  {ARG_TEST,               "help",                  parse_help},        /* GNU */
  {ARG_TEST,               "-help",                 parse_help},	/* GNU */
  {ARG_OPTION,             "ignore_readdir_race",   parse_ignore_race},	/* GNU */
  {ARG_TEST,               "ilname",                parse_ilname},	/* GNU */
  {ARG_TEST,               "iname",                 parse_iname},	/* GNU */
  {ARG_TEST,               "inum",                  parse_inum},	/* GNU, Unix */
  {ARG_TEST,               "ipath",                 parse_ipath},	/* GNU, deprecated in favour of iwholename */
  {ARG_TEST,               "iregex",                parse_iregex},	/* GNU */
  {ARG_TEST,               "iwholename",            parse_iwholename},  /* GNU */
  {ARG_TEST,               "links",                 parse_links},
  {ARG_TEST,               "lname",                 parse_lname},	/* GNU */
  {ARG_ACTION,             "ls",                    parse_ls},		/* GNU, Unix */
  {ARG_OPTION,             "maxdepth",              parse_maxdepth},	/* GNU */
  {ARG_OPTION,             "mindepth",              parse_mindepth},	/* GNU */
  {ARG_TEST,               "mmin",                  parse_mmin},	/* GNU */
  {ARG_OPTION,             "mount",                 parse_xdev},	/* Unix */
  {ARG_TEST,               "mtime",                 parse_mtime},
  {ARG_TEST,               "name",                  parse_name},
#ifdef UNIMPLEMENTED_UNIX	                    
  {ARG_UNIMPLEMENTED,      "ncpio",                 parse_ncpio},	/* Unix */
#endif				                    
  {ARG_TEST,               "newer",                 parse_newer},
  {ARG_OPTION,             "noleaf",                parse_noleaf},	/* GNU */
  {ARG_TEST,               "nogroup",               parse_nogroup},
  {ARG_TEST,               "nouser",                parse_nouser},
  {ARG_OPTION,             "noignore_readdir_race", parse_noignore_race},/* GNU */
  {ARG_OPTION,             "nowarn",                parse_nowarn},       /* GNU */
  {ARG_PUNCTUATION,        "o",                     parse_or},
  {ARG_PUNCTUATION,        "or",                    parse_or},		 /* GNU */
  {ARG_ACTION,             "ok",                    parse_ok},
  {ARG_ACTION,             "okdir",                 parse_okdir},        /* GNU (-execdir is BSD) */
  {ARG_TEST,               "path",                  parse_path},	 /* GNU, HP-UX, GNU prefers wholename */
  {ARG_TEST,               "perm",                  parse_perm},
  {ARG_ACTION,             "print",                 parse_print},
  {ARG_ACTION,             "print0",                parse_print0},	/* GNU */
  {ARG_ACTION,             "printf",                parse_printf},	/* GNU */
  {ARG_TEST,               "prune",                 parse_prune},
  {ARG_ACTION,             "quit",                  parse_quit},	/* GNU */
  {ARG_TEST,               "regex",                 parse_regex},	/* GNU */
  {ARG_TEST,               "samefile",              parse_samefile},    /* GNU */
  {ARG_TEST,               "size",                  parse_size},
  {ARG_TEST,               "true",                  parse_true},	/* GNU */
  {ARG_TEST,               "type",                  parse_type},
  {ARG_TEST,               "uid",                   parse_uid},		/* GNU */
  {ARG_TEST,               "used",                  parse_used},	/* GNU */
  {ARG_TEST,               "user",                  parse_user},
  {ARG_TEST,               "version",               parse_version},	/* GNU */
  {ARG_TEST,               "-version",              parse_version},	/* GNU */
  {ARG_OPTION,             "warn",                  parse_warn},        /* GNU */
  {ARG_TEST,               "wholename",             parse_wholename},   /* GNU, replaces -path */
  {ARG_OPTION,             "xdev",                  parse_xdev},
  {ARG_TEST,               "xtype",                 parse_xtype},	/* GNU */
  {0, 0, 0}
};


static const char *first_nonoption_arg = NULL;

/* Return a pointer to the parser function to invoke for predicate
   SEARCH_NAME.
   Return NULL if SEARCH_NAME is not a valid predicate name. */

PFB
find_parser (char *search_name)
{
  int i;
  const char *original_arg = search_name;
  
  if (*search_name == '-')
    search_name++;
  for (i = 0; parse_table[i].parser_name != 0; i++)
    {
      if (strcmp (parse_table[i].parser_name, search_name) == 0)
	{
	  /* If this is an option, but we have already had a 
	   * non-option argument, the user may be under the 
	   * impression that the behaviour of the option 
	   * argument is conditional on some preceding 
	   * tests.  This might typically be the case with,
	   * for example, -maxdepth.
	   *
	   * The options -daystart and -follow are exempt 
	   * from this treatment, since their positioning
	   * in the command line does have an effect on 
	   * subsequent tests but not previous ones.  That
	   * might be intentional on the part of the user.
	   */
	  if (parse_table[i].type != ARG_POSITIONAL_OPTION)
	    {
	      /* Something other than -follow/-daystart.
	       * If this is an option, check if it followed
	       * a non-option and if so, issue a warning.
	       */
	      if (parse_table[i].type == ARG_OPTION)
		{
		  if ((first_nonoption_arg != NULL)
		      && options.warnings )
		    {
		      /* option which folows a non-option */
		      error (0, 0,
			     _("warning: you have specified the %s "
			       "option after a non-option argument %s, "
			       "but options are not positional (%s affects "
			       "tests specified before it as well as those "
			       "specified after it).  Please specify options "
			       "before other arguments.\n"),
			     original_arg,
			     first_nonoption_arg,
			     original_arg);
		    }
		}
	      else
		{
		  /* Not an option or a positional option,
		   * so remember we've seen it in order to 
		   * use it in a possible future warning message.
		   */
		  if (first_nonoption_arg == NULL)
		    {
		      first_nonoption_arg = original_arg;
		    }
		}
	    }
	  
	  return (parse_table[i].parser_func);
	}
    }
  return NULL;
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
  t = options.cur_day_start + DAYSECS - num * 60;
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

  (void) argv;
  (void) arg_ptr;
  
  our_pred = get_new_pred ();
  our_pred->pred_func = pred_and;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_and);
#endif	/* DEBUG */
  our_pred->p_type = BI_OP;
  our_pred->p_prec = AND_PREC;
  our_pred->need_stat = our_pred->need_type = false;
  return (true);
}

static boolean
parse_anewer (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  struct stat stat_newer;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if ((*options.xstat) (argv[*arg_ptr], &stat_newer))
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

  (void) argv;
  (void) arg_ptr;
  
  our_pred = get_new_pred ();
  our_pred->pred_func = pred_close;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_close);
#endif	/* DEBUG */
  our_pred->p_type = CLOSE_PAREN;
  our_pred->p_prec = NO_PREC;
  our_pred->need_stat = our_pred->need_type = false;
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
  t = options.cur_day_start + DAYSECS - num * 60;
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
  if ((*options.xstat) (argv[*arg_ptr], &stat_newer))
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

  (void) argv;
  (void) arg_ptr;

  our_pred = get_new_pred ();
  our_pred->pred_func = pred_comma;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_comma);
#endif /* DEBUG */
  our_pred->p_type = BI_OP;
  our_pred->p_prec = COMMA_PREC;
  our_pred->need_stat = our_pred->need_type = false;
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

  (void) argv;
  (void) arg_ptr;

  if (options.full_days == false)
    {
      options.cur_day_start += DAYSECS;
      local = localtime (&options.cur_day_start);
      options.cur_day_start -= (local
			? (local->tm_sec + local->tm_min * 60
			   + local->tm_hour * 3600)
			: options.cur_day_start % DAYSECS);
      options.full_days = true;
    }
  return true;
}

static boolean
parse_delete (argv, arg_ptr)
  char *argv[];
  int *arg_ptr;
{
  struct predicate *our_pred;
  (void) argv;
  (void) arg_ptr;
  
  our_pred = insert_primary (pred_delete);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  /* -delete implies -depth */
  options.do_dir_first = false;
  return (true);
}

static boolean
parse_depth (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;

  options.do_dir_first = false;
  return (true);
}
 
static boolean
parse_d (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
  
  if (options.warnings)
    {
      error (0, 0,
	     _("warning: the -d option is deprecated; please use -depth instead, because the latter is a POSIX-compliant feature."));
    }
  return parse_depth(argv, arg_ptr);
}
 
static boolean
parse_empty (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;

  insert_primary (pred_empty);
  return (true);
}

static boolean
parse_exec (char **argv, int *arg_ptr)
{
  return (insert_exec_ok ("-exec", pred_exec, argv, arg_ptr));
}

static boolean
parse_execdir (char **argv, int *arg_ptr)
{
  return (insert_exec_ok ("-execdir", pred_execdir, argv, arg_ptr));
}

static boolean
parse_false (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  
  (void) argv;
  (void) arg_ptr;

  our_pred = insert_primary (pred_false);
  our_pred->need_stat = our_pred->need_type = false;
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
  (void) argv;
  (void) arg_ptr;

  set_follow_state(SYMLINK_ALWAYS_DEREF);
  return true;
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
  our_pred->need_stat = our_pred->need_type = false;
  (*arg_ptr)++;
  return true;
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
  our_pred->need_stat = our_pred->need_type = false;
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
  (void) argv;
  (void) arg_ptr;
  
  printf (_("\
Usage: %s [path...] [expression]\n"), program_name);
  puts (_("\n\
default path is the current directory; default expression is -print\n\
expression may consist of: operators, options, tests, and actions:\n"));
  puts (_("\
operators (decreasing precedence; -and is implicit where no others are given):\n\
      ( EXPR )   ! EXPR   -not EXPR   EXPR1 -a EXPR2   EXPR1 -and EXPR2\n\
      EXPR1 -o EXPR2   EXPR1 -or EXPR2   EXPR1 , EXPR2\n"));
  puts (_("\
positional options (always true): -daystart -follow\n\
normal options (always true, specified before other expressions):\n\
      -depth --help -maxdepth LEVELS -mindepth LEVELS -mount -noleaf\n\
      --version -xdev -ignore_readdir_race -noignore_readdir_race\n"));
  puts (_("\
tests (N can be +N or -N or N): -amin N -anewer FILE -atime N -cmin N\n\
      -cnewer FILE -ctime N -empty -false -fstype TYPE -gid N -group NAME\n\
      -ilname PATTERN -iname PATTERN -inum N -iwholename PATTERN -iregex PATTERN\n\
      -links N -lname PATTERN -mmin N -mtime N -name PATTERN -newer FILE"));
  puts (_("\
      -nouser -nogroup -path PATTERN -perm [+-]MODE -regex PATTERN\n\
      -wholename PATTERN -size N[bcwkMG] -true -type [bcdpflsD] -uid N\n\
      -used N -user NAME -xtype [bcdpfls]\n"));
  puts (_("\
actions: -exec COMMAND ; -fprint FILE -fprint0 FILE -fprintf FILE FORMAT\n\
      -fls FILE -ok COMMAND ; -print -print0 -printf FORMAT -prune -ls -delete\n\
      -quit\n"));
  puts (_("Report (and track progress on fixing) bugs via the findutils bug-reporting\n\
page at http://savannah.gnu.org/ or, if you have no web access, by sending\n\
email to <bug-findutils@gnu.org>."));
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


/* sanity check the fnmatch() function to make sure
 * it really is the GNU version. 
 */
static boolean 
fnmatch_sanitycheck()
{
  /* fprintf(stderr, "Performing find sanity check..."); */
  if (0 != fnmatch("foo", "foo", 0)
      || 0 == fnmatch("Foo", "foo", 0)
      || 0 != fnmatch("Foo", "foo", FNM_CASEFOLD))
    {
      error (1, 0, _("sanity check of the fnmatch() library function failed."));
      /* fprintf(stderr, "FAILED\n"); */
      return false;
    }

  /* fprintf(stderr, "OK\n"); */
  return true;
}



static boolean
parse_iname (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);

  fnmatch_sanitycheck();
  
  our_pred = insert_primary (pred_iname);
  our_pred->need_stat = our_pred->need_type = false;
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_inum (char **argv, int *arg_ptr)
{
  return (insert_num (argv, arg_ptr, pred_inum));
}

/* -ipath is deprecated (at RMS's request) in favour of 
 * -iwholename.   See the node "GNU Manuals" in standards.texi
 * for the rationale for this (basically, GNU prefers the use 
 * of the phrase "file name" to "path name"
 */
static boolean
parse_ipath (char **argv, int *arg_ptr)
{
  error (0, 0,
	 _("warning: the predicate -ipath is deprecated; please use -iwholename instead."));
  
  return parse_iwholename(argv, arg_ptr);
}

static boolean
parse_iwholename (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);

  fnmatch_sanitycheck();
  
  our_pred = insert_primary (pred_ipath);
  our_pred->need_stat = our_pred->need_type = false;
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

  (void) argv;
  (void) arg_ptr;
  
  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);

  fnmatch_sanitycheck();
  
  our_pred = insert_primary (pred_lname);
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_ls (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  (void) &argv;
  (void) &arg_ptr;

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
    return false;
  depth_len = strspn (argv[*arg_ptr], "0123456789");
  if ((depth_len == 0) || (argv[*arg_ptr][depth_len] != '\0'))
    return false;
  options.maxdepth = atoi (argv[*arg_ptr]);
  if (options.maxdepth < 0)
    return false;
  (*arg_ptr)++;
  return true;
}

static boolean
parse_mindepth (char **argv, int *arg_ptr)
{
  int depth_len;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return false;
  depth_len = strspn (argv[*arg_ptr], "0123456789");
  if ((depth_len == 0) || (argv[*arg_ptr][depth_len] != '\0'))
    return false;
  options.mindepth = atoi (argv[*arg_ptr]);
  if (options.mindepth < 0)
    return false;
  (*arg_ptr)++;
  return true;
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
  t = options.cur_day_start + DAYSECS - num * 60;
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

  (void) argv;
  (void) arg_ptr;
  
  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_name);
  our_pred->need_stat = our_pred->need_type = false;
  our_pred->args.str = argv[*arg_ptr];
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_negate (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  (void) &argv;
  (void) &arg_ptr;

  our_pred = get_new_pred_chk_op ();
  our_pred->pred_func = pred_negate;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_negate);
#endif	/* DEBUG */
  our_pred->p_type = UNI_OP;
  our_pred->p_prec = NEGATE_PREC;
  our_pred->need_stat = our_pred->need_type = false;
  return (true);
}

static boolean
parse_newer (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  struct stat stat_newer;

  (void) argv;
  (void) arg_ptr;
  
  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if ((*options.xstat) (argv[*arg_ptr], &stat_newer))
    error (1, errno, "%s", argv[*arg_ptr]);
  our_pred = insert_primary (pred_newer);
  our_pred->args.time = stat_newer.st_mtime;
  (*arg_ptr)++;
  return (true);
}

static boolean
parse_noleaf (char **argv, int *arg_ptr)
{
  (void) &argv;
  (void) &arg_ptr;
  
  options.no_leaf_check = true;
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

  (void) &argv;
  (void) &arg_ptr;
  
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
  (void) argv;
  (void) arg_ptr;
  

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
parse_nowarn (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
  
  options.warnings = false;
  return true;;
}

static boolean
parse_ok (char **argv, int *arg_ptr)
{
  return (insert_exec_ok ("-ok", pred_ok, argv, arg_ptr));
}

static boolean
parse_okdir (char **argv, int *arg_ptr)
{
  return (insert_exec_ok ("-okdir", pred_okdir, argv, arg_ptr));
}

boolean
parse_open (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  (void) argv;
  (void) arg_ptr;
  
  our_pred = get_new_pred_chk_op ();
  our_pred->pred_func = pred_open;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_open);
#endif	/* DEBUG */
  our_pred->p_type = OPEN_PAREN;
  our_pred->p_prec = NO_PREC;
  our_pred->need_stat = our_pred->need_type = false;
  return (true);
}

static boolean
parse_or (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  (void) argv;
  (void) arg_ptr;
  
  our_pred = get_new_pred ();
  our_pred->pred_func = pred_or;
#ifdef	DEBUG
  our_pred->p_name = find_pred_name (pred_or);
#endif	/* DEBUG */
  our_pred->p_type = BI_OP;
  our_pred->p_prec = OR_PREC;
  our_pred->need_stat = our_pred->need_type = false;
  return (true);
}

/* -path is deprecated (at RMS's request) in favour of 
 * -iwholename.   See the node "GNU Manuals" in standards.texi
 * for the rationale for this (basically, GNU prefers the use 
 * of the phrase "file name" to "path name".
 *
 * We do not issue a warning that this usage is deprecated
 * since HPUX find supports this predicate also.
 */
static boolean
parse_path (char **argv, int *arg_ptr)
{
  return parse_wholename(argv, arg_ptr);
}

static boolean
parse_wholename (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  our_pred = insert_primary (pred_path);
  our_pred->need_stat = our_pred->need_type = false;
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

  (void) argv;
  (void) arg_ptr;
  
  our_pred = insert_primary (pred_print);
  /* -print has the side effect of printing.  This prevents us
     from doing undesired multiple printing when the user has
     already specified -print. */
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->need_stat = our_pred->need_type = false;
  return (true);
}

static boolean
parse_print0 (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  (void) argv;
  (void) arg_ptr;
  
  our_pred = insert_primary (pred_print0);
  /* -print0 has the side effect of printing.  This prevents us
     from doing undesired multiple printing when the user has
     already specified -print0. */
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  our_pred->need_stat = our_pred->need_type = false;
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

  (void) argv;
  (void) arg_ptr;
  
  our_pred = insert_primary (pred_prune);
  our_pred->need_stat = our_pred->need_type = false;
  /* -prune has a side effect that it does not descend into
     the current directory. */
  our_pred->side_effects = true;
  return (true);
}

static boolean 
parse_quit  (char **argv, int *arg_ptr)
{
  struct predicate *our_pred = insert_primary (pred_quit);
  (void) argv;
  (void) arg_ptr;
  our_pred->need_stat = our_pred->need_type = false;
  return true;
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
  our_pred->need_stat = our_pred->need_type = false;
  re = (struct re_pattern_buffer *)
    xmalloc (sizeof (struct re_pattern_buffer));
  our_pred->args.regex = re;
  re->allocated = 100;
  re->buffer = (unsigned char *) xmalloc (re->allocated);
  re->fastmap = NULL;
  
  if (ignore_case)
    {
      re_syntax_options |= RE_ICASE;
    }
  else
    {
      re_syntax_options &= ~RE_ICASE;
    }
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

    case 'M':			/* Megabytes */
      blksize = 1024*1024;
      argv[*arg_ptr][len - 1] = '\0';
      break;

    case 'G':			/* Gigabytes */
      blksize = 1024*1024*1024;
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
parse_samefile (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;
  struct stat st;
  
  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);
  if ((*options.xstat) (argv[*arg_ptr], &st))
    error (1, errno, "%s", argv[*arg_ptr]);
  
  our_pred = insert_primary (pred_samefile);
  our_pred->args.fileid.ino = st.st_ino;
  our_pred->args.fileid.dev = st.st_dev;
  our_pred->need_type = false;
  our_pred->need_stat = true;
  (*arg_ptr)++;
  return (true);
}


static boolean
parse_true (char **argv, int *arg_ptr)
{
  struct predicate *our_pred;

  (void) argv;
  (void) arg_ptr;
  
  our_pred = insert_primary (pred_true);
  our_pred->need_stat = our_pred->need_type = false;
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
  int features = 0;
  
  (void) argv;
  (void) arg_ptr;
  
  fflush (stderr);
  printf (_("GNU find version %s\n"), version_string);
  printf (_("Features enabled: "));
  
#if CACHE_IDS
  printf("CACHE_IDS ");
  ++features;
#endif
#if DEBUG
  printf("DEBUG ");
  ++features;
#endif
#if DEBUG_STAT
  printf("DEBUG_STAT ");
  ++features;
#endif
#if defined(USE_STRUCT_DIRENT_D_TYPE) && defined(HAVE_STRUCT_DIRENT_D_TYPE)
  printf("D_TYPE ");
  ++features;
#endif
#if defined(O_NOFOLLOW)
  printf("O_NOFOLLOW(%s) ",
	 (options.open_nofollow_available ? "enabled" : "disabled"));
  ++features;
#endif
  
  if (0 == features)
    {
      /* For the moment, leave this as English in case someone wants
	 to parse these strings. */
      printf("none");
    }
  printf("\n");
  
  exit (0);
}

static boolean
parse_xdev (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
  options.stay_on_filesystem = true;
  return true;
}

static boolean
parse_ignore_race (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
  options.ignore_readdir_race = true;
  return true;
}

static boolean
parse_noignore_race (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
  options.ignore_readdir_race = false;
  return true;
}

static boolean
parse_warn (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
  options.warnings = true;
  return true;
}

static boolean
parse_xtype (char **argv, int *arg_ptr)
{
  (void) argv;
  (void) arg_ptr;
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

  /* Figure out if we will need to stat the file, because if we don't
   * need to follow symlinks, we can avoid a stat call by using 
   * struct dirent.d_type.
   */
  if (which_pred == pred_xtype)
    {
      our_pred->need_stat = true;
      our_pred->need_type = false;
    }
  else
    {
      our_pred->need_stat = false; /* struct dirent is enough */
      our_pred->need_type = true;
    }
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
	  if (strchr ("abcdDfFgGhHiklmMnpPstuUyY", *scan2))
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
  our_pred->need_type = false;
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
    case 'c':			/* ctime in `ctime' format */
    case 'C':			/* ctime in user-specified strftime format */
    case 'F':			/* filesystem type */
    case 'g':			/* group name */
    case 'i':			/* inode number */
    case 'l':			/* object of symlink */
    case 'M':			/* mode in `ls -l' format (eg., "drwxr-xr-x") */
    case 's':			/* size in bytes */
    case 't':			/* mtime in `ctime' format */
    case 'T':			/* mtime in user-specified strftime format */
    case 'u':			/* user name */
    case 'y':			/* file type */
    case 'Y':			/* symlink pointed file type */
      fprintf_stat_needed = true;
      /* FALLTHROUGH */
    case 'f':			/* basename of path */
    case 'h':			/* leading directories part of path */
    case 'H':			/* ARGV element file was found under */
    case 'p':			/* pathname */
    case 'P':			/* pathname with ARGV element stripped */
      *fmt++ = 's';
      break;

      /* Numeric items that one might expect to honour 
       * #, 0, + flags but which do not.
       */
    case 'G':			/* GID number */
    case 'U':			/* UID number */
    case 'b':			/* size in 512-byte blocks */
    case 'D':                   /* Filesystem device on which the file exits */
    case 'k':			/* size in 1K blocks */
    case 'n':			/* number of links */
      fprintf_stat_needed = true;
      *fmt++ = 's';
      break;
      
      /* Numeric items that DO honour #, 0, + flags.
       */
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

static void 
check_path_safety(const char *action)
{
  const char *path = getenv("PATH");
  char *s;
  s = next_element(path, 1);
  while ((s = next_element ((char *) NULL, 1)) != NULL)
    {
      if (0 == strcmp(s, "."))
	{
	  error(1, 0, _("The current directory is included in the PATH environment variable, which is insecure in combination with the %s action of find.  Please remove the current directory from your $PATH (that is, remove \".\" or leading or trailing colons)"),
		action);
	}
    }
}


/* handles both exec and ok predicate */
#if defined(NEW_EXEC)
/* handles both exec and ok predicate */
static boolean
new_insert_exec_ok (const char *action,
		    boolean (*func) (/* ??? */),
		    char **argv, int *arg_ptr)
{
  int start, end;		/* Indexes in ARGV of start & end of cmd. */
  int i;			/* Index into cmd args */
  int saw_braces;		/* True if previous arg was '{}'. */
  boolean allow_plus;		/* True if + is a valid terminator */
  int brace_count;		/* Number of instances of {}. */
  
  struct predicate *our_pred;
  struct exec_val *execp;	/* Pointer for efficiency. */

  if ((argv == NULL) || (argv[*arg_ptr] == NULL))
    return (false);

  our_pred = insert_primary (func);
  our_pred->side_effects = true;
  our_pred->no_default_print = true;
  execp = &our_pred->args.exec_vec;

  if ((func != pred_okdir) && (func != pred_ok))
    allow_plus = true;
  else
    allow_plus = false;
  
  if ((func == pred_execdir) || (func == pred_okdir))
    {
      options.ignore_readdir_race = false;
      check_path_safety(action);
      execp->use_current_dir = true;
    }
  else
    {
      execp->use_current_dir = false;
    }
  
  our_pred->args.exec_vec.multiple = 0;

  /* Count the number of args with path replacements, up until the ';'. 
   * Also figure out if the command is terminated by ";" or by "+".
   */
  start = *arg_ptr;
  for (end = start, saw_braces=0, brace_count=0;
       (argv[end] != NULL)
       && ((argv[end][0] != ';') || (argv[end][1] != '\0'));
       end++)
    {
      /* For -exec and -execdir, "{} +" can terminate the command. */
      if ( allow_plus
	   && argv[end][0] == '+' && argv[end][1] == 0
	   && saw_braces)
	{
	  our_pred->args.exec_vec.multiple = 1;
	  break;
	}
      
      saw_braces = 0;
      if (strstr (argv[end], "{}"))
	{
	  saw_braces = 1;
	  ++brace_count;
	  
	  if (0 == end && (func == pred_execdir || func == pred_okdir))
	    {
	      /* The POSIX standard says that {} replacement should
	       * occur even in the utility name.  This is insecure
	       * since it means we will be executing a command whose
	       * name is chosen according to whatever find finds in
	       * the filesystem.  That can be influenced by an
	       * attacker.  Hence for -execdir and -okdir this is not
	       * allowed.  We can specify this as those options are 
	       * not defined by POSIX.
	       */
	      error(1, 0, _("You may not use {} within the utility name for -execdir and -okdir, because this is a potential security problem."));
	    }
	}
    }
  
  /* Fail if no command given or no semicolon found. */
  if ((end == start) || (argv[end] == NULL))
    {
      *arg_ptr = end;
      free(our_pred);
      return false;
    }

  if (our_pred->args.exec_vec.multiple && brace_count > 1)
    {
	
      const char *suffix;
      if (func == pred_execdir)
	suffix = "dir";
      else
	suffix = "";

      error(1, 0,
	    _("Only one instance of {} is supported with -exec%s ... +"),
	    suffix);
    }

  /* execp->ctl   = xmalloc(sizeof struct buildcmd_control); */
  bc_init_controlinfo(&execp->ctl);
  execp->ctl.exec_callback = launch;

  if (our_pred->args.exec_vec.multiple)
    {
      /* "+" terminator, so we can just append our arguments after the
       * command and initial arguments.
       */
      execp->replace_vec = NULL;
      execp->ctl.replace_pat = NULL;
      execp->ctl.rplen = 0;
      execp->ctl.lines_per_exec = 0; /* no limit */
      execp->ctl.args_per_exec = 0; /* no limit */
      
      /* remember how many arguments there are */
      execp->ctl.initial_argc = (end-start) - 1;

      /* execp->state = xmalloc(sizeof struct buildcmd_state); */
      bc_init_state(&execp->ctl, &execp->state, execp);
  
      /* Gather the initial arguments.  Skip the {}. */
      for (i=start; i<end-1; ++i)
	{
	  bc_push_arg(&execp->ctl, &execp->state,
		      argv[i], strlen(argv[i])+1,
		      NULL, 0,
		      1);
	}
    }
  else
    {
      /* Semicolon terminator - more than one {} is supported, so we
       * have to do brace-replacement.
       */
      execp->num_args = end - start;
      
      execp->ctl.replace_pat = "{}";
      execp->ctl.rplen = strlen(execp->ctl.replace_pat);
      execp->ctl.lines_per_exec = 0; /* no limit */
      execp->ctl.args_per_exec = 0; /* no limit */
      execp->replace_vec = xmalloc(sizeof(char*)*execp->num_args);


      /* execp->state = xmalloc(sizeof(*(execp->state))); */
      bc_init_state(&execp->ctl, &execp->state, execp);

      /* Remember the (pre-replacement) arguments for later. */
      for (i=0; i<execp->num_args; ++i)
	{
	  execp->replace_vec[i] = argv[i+start];
	}
    }
  
  if (argv[end] == NULL)
    *arg_ptr = end;
  else
    *arg_ptr = end + 1;
  
  return true;
}
#else
/* handles both exec and ok predicate */
static boolean
old_insert_exec_ok (boolean (*func) (/* ??? */), char **argv, int *arg_ptr)
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
  execp->usercontext = our_pred;
  execp->use_current_dir = false;
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
#endif



static boolean
insert_exec_ok (const char *action,
		boolean (*func) (/* ??? */), char **argv, int *arg_ptr)
{
#if defined(NEW_EXEC)
  return new_insert_exec_ok(action, func, argv, arg_ptr);
#else
  return old_insert_exec_ok(func, argv, arg_ptr);
#endif
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

  /* Figure out the timestamp value we are looking for. */
  t = ( options.cur_day_start - num_days * DAYSECS
		   + ((c_type == COMP_GT) ? DAYSECS - 1 : 0));

  if (1)
    {
      /* We introduce a scope in which 'val' can be declared, for the 
       * benefit of compilers that are really C89 compilers
       * which support intmax_t because config.h #defines it
       */
      intmax_t val = ( (intmax_t)options.cur_day_start - num_days * DAYSECS
		       + ((c_type == COMP_GT) ? DAYSECS - 1 : 0));
      t = val;
      
      /* Check for possibility of an overflow */
      if ( (intmax_t)t != val ) 
	{
	  error (1, 0, "arithmetic overflow while converting %s days to a number of seconds", argv[*arg_ptr]);
	}
    }
  
  our_pred = insert_primary (pred);
  our_pred->args.info.kind = c_type;
  our_pred->args.info.negative = t < 0;
  our_pred->args.info.l_val = t;
  (*arg_ptr)++;
#ifdef	DEBUG
  fprintf (stderr, _("inserting %s\n"), our_pred->p_name);
  fprintf (stderr, _("    type: %s    %s  "),
	  (c_type == COMP_GT) ? "gt" :
	  ((c_type == COMP_LT) ? "lt" : ((c_type == COMP_EQ) ? "eq" : "?")),
	  (c_type == COMP_GT) ? " >" :
	  ((c_type == COMP_LT) ? " <" : ((c_type == COMP_EQ) ? ">=" : " ?")));
  t = our_pred->args.info.l_val;
  fprintf (stderr, "%ju %s", (uintmax_t) our_pred->args.info.l_val, ctime (&t));
  if (c_type == COMP_EQ)
    {
      t = our_pred->args.info.l_val += DAYSECS;
      fprintf (stderr,
	       "                 <  %ju %s",
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
  fprintf (stderr, _("inserting %s\n"), our_pred->p_name);
  fprintf (stderr, _("    type: %s    %s  "),
	  (c_type == COMP_GT) ? "gt" :
	  ((c_type == COMP_LT) ? "lt" : ((c_type == COMP_EQ) ? "eq" : "?")),
	  (c_type == COMP_GT) ? " >" :
	  ((c_type == COMP_LT) ? " <" : ((c_type == COMP_EQ) ? " =" : " ?")));
  fprintf (stderr, "%ju\n", our_pred->args.info.l_val);
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
