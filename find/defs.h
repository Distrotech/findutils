/* defs.h -- data types and declarations.
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

#include <gnulib/config.h>
#undef VERSION
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE
#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#include <string.h>
#else
#include <strings.h>
#ifndef strchr
#define strchr index
#endif
#ifndef strrchr
#define strrchr rindex
#endif
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <time.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif
#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include "regex.h"

#ifndef S_IFLNK
#define lstat stat
#endif

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

int lstat PARAMS((const char *__path, struct stat *__statbuf));
int stat PARAMS((const char *__path, struct stat *__statbuf));

#ifndef S_ISUID
# define S_ISUID 0004000
#endif
#ifndef S_ISGID
# define S_ISGID 0002000
#endif
#ifndef S_ISVTX
# define S_ISVTX 0001000
#endif
#ifndef S_IRUSR
# define S_IRUSR 0000400
#endif
#ifndef S_IWUSR
# define S_IWUSR 0000200
#endif
#ifndef S_IXUSR
# define S_IXUSR 0000100
#endif
#ifndef S_IRGRP
# define S_IRGRP 0000040
#endif
#ifndef S_IWGRP
# define S_IWGRP 0000020
#endif
#ifndef S_IXGRP
# define S_IXGRP 0000010
#endif
#ifndef S_IROTH
# define S_IROTH 0000004
#endif
#ifndef S_IWOTH
# define S_IWOTH 0000002
#endif
#ifndef S_IXOTH
# define S_IXOTH 0000001
#endif

#define MODE_WXUSR	(S_IWUSR | S_IXUSR)
#define MODE_R		(S_IRUSR | S_IRGRP | S_IROTH)
#define MODE_RW		(S_IWUSR | S_IWGRP | S_IWOTH | MODE_R)
#define MODE_RWX	(S_IXUSR | S_IXGRP | S_IXOTH | MODE_RW)
#define MODE_ALL	(S_ISUID | S_ISGID | S_ISVTX | MODE_RWX)

/* Not char because of type promotion; NeXT gcc can't handle it.  */
typedef int boolean;
#define		true    1
#define		false	0

/* Pointer to function returning boolean. */
typedef boolean (*PFB)();

/* The number of seconds in a day. */
#define		DAYSECS	    86400

/* Argument structures for predicates. */

enum comparison_type
{
  COMP_GT,
  COMP_LT,
  COMP_EQ
};

enum permissions_type
{
  PERM_AT_LEAST,
  PERM_ANY,
  PERM_EXACT
};

enum predicate_type
{
  NO_TYPE,
  PRIMARY_TYPE,
  UNI_OP,
  BI_OP,
  OPEN_PAREN,
  CLOSE_PAREN
};

enum predicate_precedence
{
  NO_PREC,
  COMMA_PREC,
  OR_PREC,
  AND_PREC,
  NEGATE_PREC,
  MAX_PREC
};

struct long_val
{
  enum comparison_type kind;
  boolean negative;		/* Defined only when representing time_t.  */
  uintmax_t l_val;
};

struct perm_val
{
  enum permissions_type kind;
  mode_t val;
};

struct size_val
{
  enum comparison_type kind;
  int blocksize;
  uintmax_t size;
};

struct path_arg
{
  short offset;			/* Offset in `vec' of this arg. */
  short count;			/* Number of path replacements in this arg. */
  char *origarg;		/* Arg with "{}" intact. */
};

struct exec_val
{
  struct path_arg *paths;	/* Array of args with path replacements. */
  char **vec;			/* Array of args to pass to program. */
};

/* The format string for a -printf or -fprintf is chopped into one or
   more `struct segment', linked together into a list.
   Each stretch of plain text is a segment, and
   each \c and `%' conversion is a segment. */

/* Special values for the `kind' field of `struct segment'. */
#define KIND_PLAIN 0		/* Segment containing just plain text. */
#define KIND_STOP 1		/* \c -- stop printing and flush output. */

struct segment
{
  int kind;			/* Format chars or KIND_{PLAIN,STOP}. */
  char *text;			/* Plain text or `%' format string. */
  int text_len;			/* Length of `text'. */
  struct segment *next;		/* Next segment for this predicate. */
};

struct format_val
{
  struct segment *segment;	/* Linked list of segments. */
  FILE *stream;			/* Output stream to print on. */
};

struct predicate
{
  /* Pointer to the function that implements this predicate.  */
  PFB pred_func;

  /* Only used for debugging, but defined unconditionally so individual
     modules can be compiled with -DDEBUG.  */
  char *p_name;

  /* The type of this node.  There are two kinds.  The first is real
     predicates ("primaries") such as -perm, -print, or -exec.  The
     other kind is operators for combining predicates. */
  enum predicate_type p_type;

  /* The precedence of this node.  Only has meaning for operators. */
  enum predicate_precedence p_prec;

  /* True if this predicate node produces side effects.
     If side_effects are produced
     then optimization will not be performed */
  boolean side_effects;

  /* True if this predicate node requires default print be turned off. */
  boolean no_default_print;

  /* True if this predicate node requires a stat system call to execute. */
  boolean need_stat;

  /* Information needed by the predicate processor.
     Next to each member are listed the predicates that use it. */
  union
  {
    char *str;			/* fstype [i]lname [i]name [i]path */
    struct re_pattern_buffer *regex; /* regex */
    struct exec_val exec_vec;	/* exec ok */
    struct long_val info;	/* atime ctime gid inum links mtime
                                   size uid */
    struct size_val size;	/* size */
    uid_t uid;			/* user */
    gid_t gid;			/* group */
    time_t time;		/* newer */
    struct perm_val perm;	/* perm */
    mode_t type;		/* type */
    FILE *stream;		/* fprint fprint0 */
    struct format_val printf_vec; /* printf fprintf */
  } args;

  /* The next predicate in the user input sequence,
     which repesents the order in which the user supplied the
     predicates on the command line. */
  struct predicate *pred_next;

  /* The right and left branches from this node in the expression
     tree, which represents the order in which the nodes should be
     processed. */
  struct predicate *pred_left;
  struct predicate *pred_right;
};

/* find library function declarations.  */

/* dirname.c */
char *dirname PARAMS((char *path));

/* error.c */
void error PARAMS((int status, int errnum, char *message, ...));

/* listfile.c */
void list_file PARAMS((char *name, char *relname, struct stat *statp, time_t current_time, int output_block_size, FILE *stream));
char *get_link_name PARAMS((char *name, char *relname));

/* stpcpy.c */
#if !HAVE_STPCPY
char *stpcpy PARAMS((char *dest, const char *src));
#endif

/* xgetcwd.c */
char *xgetcwd PARAMS((void));

/* xmalloc.c */
#if __STDC__
#define VOID void
#else
#define VOID char
#endif

VOID *xmalloc PARAMS((size_t n));
VOID *xrealloc PARAMS((VOID *p, size_t n));

/* xstrdup.c */
char *xstrdup PARAMS((char *string));

/* find global function declarations.  */

/* fstype.c */
char *filesystem_type PARAMS((char *path, char *relpath, struct stat *statp));

/* parser.c */
PFB find_parser PARAMS((char *search_name));
boolean parse_close PARAMS((char *argv[], int *arg_ptr));
boolean parse_open PARAMS((char *argv[], int *arg_ptr));
boolean parse_print PARAMS((char *argv[], int *arg_ptr));

/* pred.c */
boolean pred_amin PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_and PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_anewer PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_atime PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_close PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_cmin PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_cnewer PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_comma PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_ctime PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_empty PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_exec PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_false PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_fls PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_fprint PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_fprint0 PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_fprintf PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_fstype PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_gid PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_group PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_ilname PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_iname PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_inum PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_ipath PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_links PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_lname PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_ls PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_mmin PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_mtime PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_name PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_negate PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_newer PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_nogroup PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_nouser PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_ok PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_open PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_or PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_path PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_perm PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_print PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_print0 PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_prune PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_regex PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_size PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_true PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_type PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_uid PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_used PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_user PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
boolean pred_xtype PARAMS((char *pathname, struct stat *stat_buf, struct predicate *pred_ptr));
char *find_pred_name PARAMS((PFB pred_func));
#ifdef DEBUG
void print_tree PARAMS((struct predicate *node, int indent));
void print_list PARAMS((struct predicate *node));
#endif /* DEBUG */

/* tree.c */
struct predicate *
get_expr PARAMS((struct predicate **input, short int prev_prec));
boolean opt_expr PARAMS((struct predicate **eval_treep));
boolean mark_stat PARAMS((struct predicate *tree));

/* util.c */
struct predicate *get_new_pred PARAMS((void));
struct predicate *get_new_pred_chk_op PARAMS((void));
struct predicate *insert_primary PARAMS((boolean (*pred_func )()));
void usage PARAMS((char *msg));

extern char *program_name;
extern struct predicate *predicates;
extern struct predicate *last_pred;
extern boolean do_dir_first;
extern int maxdepth;
extern int mindepth;
extern int curdepth;
extern int output_block_size;
extern time_t start_time;
extern time_t cur_day_start;
extern boolean full_days;
extern boolean no_leaf_check;
extern boolean stay_on_filesystem;
extern boolean stop_at_current_level;
extern boolean have_stat;
extern char *rel_pathname;
extern char const *starting_dir;
extern int starting_desc;
#if ! defined HAVE_FCHDIR && ! defined fchdir
# define fchdir(fd) (-1)
#endif
extern int exit_status;
extern int path_length;
extern int (*xstat) ();
extern boolean dereference;
