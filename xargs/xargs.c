/* xargs -- build and execute command lines from standard input
   Copyright (C) 1990, 91, 92, 93, 94, 2000, 2003, 2005 Free Software Foundation, Inc.

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
   USA.
*/

/* Written by Mike Rendell <michael@cs.mun.ca>
   and David MacKenzie <djm@gnu.org>.  */

#include <config.h>

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

#include <ctype.h>

#if !defined (isascii) || defined (STDC_HEADERS)
#ifdef isascii
#undef isascii
#endif
#define isascii(c) 1
#endif

#ifdef isblank
#define ISBLANK(c) (isascii (c) && isblank (c))
#else
#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#endif

#define ISSPACE(c) (ISBLANK (c) || (c) == '\n' || (c) == '\r' \
		    || (c) == '\f' || (c) == '\v')

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>

#if defined(STDC_HEADERS)
#include <assert.h>
#endif

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#include <string.h>
#if !defined(STDC_HEADERS)
#include <memory.h>
#endif
#else
#include <strings.h>
#define memcpy(dest, source, count) (bcopy((source), (dest), (count)))
#endif

#ifndef _POSIX_SOURCE
#include <sys/param.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef LONG_MAX
#define LONG_MAX (~(1 << (sizeof (long) * 8 - 1)))
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <signal.h>

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif

#include "wait.h"

/* States for read_line. */
#define NORM 0
#define SPACE 1
#define QUOTE 2
#define BACKSLASH 3

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern int errno;
#endif

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

#include "buildcmd.h"


/* Return nonzero if S is the EOF string.  */
#define EOF_STR(s) (eof_str && *eof_str == *s && !strcmp (eof_str, s))

extern char **environ;

/* Do multibyte processing if multibyte characters are supported,
   unless multibyte sequences are search safe.  Multibyte sequences
   are search safe if searching for a substring using the byte
   comparison function 'strstr' gives no false positives.  All 8-bit
   encodings and the UTF-8 multibyte encoding are search safe, but
   the EUC encodings are not.
   BeOS uses the UTF-8 encoding exclusively, so it is search safe. */
#if defined __BEOS__
# define MULTIBYTE_IS_SEARCH_SAFE 1
#endif
#define DO_MULTIBYTE (HAVE_MBLEN && ! MULTIBYTE_IS_SEARCH_SAFE)

#if DO_MULTIBYTE
# if HAVE_MBRLEN
#  include <wchar.h>
# else
   /* Simulate mbrlen with mblen as best we can.  */
#  define mbstate_t int
#  define mbrlen(s, n, ps) mblen (s, n)
# endif
#endif

/* Not char because of type promotion; NeXT gcc can't handle it.  */
typedef int boolean;
#define		true    1
#define		false	0

#if __STDC__
#define VOID void
#else
#define VOID char
#endif

#include <xalloc.h>
#include "closeout.h"

void error PARAMS ((int status, int errnum, char *message,...));

extern char *version_string;

/* The name this program was run with.  */
char *program_name;

static FILE *input_stream;

/* Buffer for reading arguments from input.  */
static char *linebuf;

static int keep_stdin = 0;

/* Line number in stdin since the last command was executed.  */
static int lineno = 0;

static struct buildcmd_state bc_state;
static struct buildcmd_control bc_ctl;


#if 0
/* If nonzero, then instead of putting the args from stdin at
   the end of the command argument list, they are each stuck into the
   initial args, replacing each occurrence of the `replace_pat' in the
   initial args.  */
static char *replace_pat = NULL;


/* The length of `replace_pat'.  */
static size_t rplen = 0;
#endif

/* If nonzero, when this string is read on stdin it is treated as
   end of file.
   IEEE Std 1003.1, 2004 Edition allows this to be NULL.
   In findutils releases up to and including 4.2.8, this was "_".
*/
static char *eof_str = NULL;

#if 0
/* If nonzero, the maximum number of nonblank lines from stdin to use
   per command line.  */
static long lines_per_exec = 0;

/* The maximum number of arguments to use per command line.  */
static long args_per_exec = 1024;

/* If true, exit if lines_per_exec or args_per_exec is exceeded.  */
static boolean exit_if_size_exceeded = false;
/* The maximum number of characters that can be used per command line.  */
static long arg_max;
/* Storage for elements of `cmd_argv'.  */
static char *argbuf;
#endif

#if 0
/* The list of args being built.  */
static char **cmd_argv = NULL;

/* Number of elements allocated for `cmd_argv'.  */
static int cmd_argv_alloc = 0;
#endif

#if 0
/* Number of valid elements in `cmd_argv'.  */
static int cmd_argc = 0;
/* Number of chars being used in `cmd_argv'.  */
static int cmd_argv_chars = 0;

/* Number of initial arguments given on the command line.  */
static int initial_argc = 0;
#endif

/* Number of chars in the initial args.  */
/* static int initial_argv_chars = 0; */

/* true when building up initial arguments in `cmd_argv'.  */
static boolean initial_args = true;

/* If nonzero, the maximum number of child processes that can be running
   at once.  */
static int proc_max = 1;

/* Total number of child processes that have been executed.  */
static int procs_executed = 0;

/* The number of elements in `pids'.  */
static int procs_executing = 0;

/* List of child processes currently executing.  */
static pid_t *pids = NULL;

/* The number of allocated elements in `pids'. */
static int pids_alloc = 0;

/* Exit status; nonzero if any child process exited with a
   status of 1-125.  */
static int child_error = 0;

/* If true, print each command on stderr before executing it.  */
static boolean print_command = false; /* Option -t */

/* If true, query the user before executing each command, and only
   execute the command if the user responds affirmatively.  */
static boolean query_before_executing = false;

static struct option const longopts[] =
{
  {"null", no_argument, NULL, '0'},
  {"arg-file", required_argument, NULL, 'a'},
  {"eof", optional_argument, NULL, 'e'},
  {"replace", optional_argument, NULL, 'I'},
  {"max-lines", optional_argument, NULL, 'l'},
  {"max-args", required_argument, NULL, 'n'},
  {"interactive", no_argument, NULL, 'p'},
  {"no-run-if-empty", no_argument, NULL, 'r'},
  {"max-chars", required_argument, NULL, 's'},
  {"verbose", no_argument, NULL, 't'},
  {"show-limits", no_argument, NULL, 'S'},
  {"exit", no_argument, NULL, 'x'},
  {"max-procs", required_argument, NULL, 'P'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {NULL, no_argument, NULL, 0}
};

static int read_line PARAMS ((void));
static int read_string PARAMS ((void));
#if 0
static char *mbstrstr PARAMS ((const char *haystack, const char *needle));
static void do_insert PARAMS ((char *arg, size_t arglen, size_t lblen));
static void push_arg PARAMS ((char *arg, size_t len));
#endif
static boolean print_args PARAMS ((boolean ask));
/* static void do_exec PARAMS ((void)); */
static int xargs_do_exec (const struct buildcmd_control *cl, struct buildcmd_state *state);
static void exec_if_possible PARAMS ((void));
static void add_proc PARAMS ((pid_t pid));
static void wait_for_proc PARAMS ((boolean all));
static void wait_for_proc_all PARAMS ((void));
static long parse_num PARAMS ((char *str, int option, long min, long max, int fatal));
static long env_size PARAMS ((char **envp));
static void usage PARAMS ((FILE * stream));



static long
get_line_max(void)
{
  long val;
#ifdef _SC_LINE_MAX  
  val = sysconf(_SC_LINE_MAX);
#else
  val = -1;
#endif
  
  if (val > 0)
    return val;

  /* either _SC_LINE_MAX was not available or 
   * there is no particular limit.
   */
#ifdef LINE_MAX
  val = LINE_MAX;
#endif

  if (val > 0)
    return val;

  return 2048L;			/* a reasonable guess. */
}


int
main (int argc, char **argv)
{
  int optc;
  int show_limits = 0;			/* --show-limits */
  int always_run_command = 1;
  char *input_file = "-"; /* "-" is stdin */
  long posix_arg_size_max;
  long posix_arg_size_min;
  long arg_size;
  long size_of_environment = env_size(environ);
  char *default_cmd = "/bin/echo";
  int (*read_args) PARAMS ((void)) = read_line;

  program_name = argv[0];

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  atexit (close_stdout);
  atexit (wait_for_proc_all);

  /* IEE Std 1003.1, 2003 specifies that the combined argument and 
   * environment list shall not exceed {ARG_MAX}-2048 bytes.  It also 
   * specifies that it shall be at least LINE_MAX.
   */
  posix_arg_size_min = get_line_max();
  posix_arg_size_max = bc_get_arg_max();
  posix_arg_size_max -= 2048; /* POSIX.2 requires subtracting 2048.  */

  bc_init_controlinfo(&bc_ctl);
  assert(bc_ctl.arg_max == posix_arg_size_max);

  bc_ctl.exec_callback = xargs_do_exec;

  
  /* Start with a reasonable default size, though this can be
   * adjusted via the -s option.
   */
  arg_size = (128 * 1024) + size_of_environment;

  /* Take the size of the environment into account.  */
  if (size_of_environment > posix_arg_size_max)
    {
      error (1, 0, _("environment is too large for exec"));
    }
  else
    {
      bc_ctl.arg_max = posix_arg_size_max - size_of_environment;
    }

  /* Check against the upper and lower limits. */  
  if (arg_size > bc_ctl.arg_max)
    arg_size = bc_ctl.arg_max;
  if (arg_size < posix_arg_size_min)
    arg_size = posix_arg_size_min;
  
  

  
  while ((optc = getopt_long (argc, argv, "+0a:E:e::i::I:l::L:n:prs:txP:",
			      longopts, (int *) 0)) != -1)
    {
      switch (optc)
	{
	case '0':
	  read_args = read_string;
	  break;

	case 'E':		/* POSIX */
	case 'e':		/* deprecated */
	  if (optarg && (strlen(optarg) > 0))
	    eof_str = optarg;
	  else
	    eof_str = 0;
	  break;

	case 'h':
	  usage (stdout);
	  return 0;

	case 'I':		/* POSIX */
	case 'i':		/* deprecated */
	  if (optarg)
	    bc_ctl.replace_pat = optarg;
	  else
	    bc_ctl.replace_pat = "{}";
	  /* -i excludes -n -l.  */
	  bc_ctl.args_per_exec = 0;
	  bc_ctl.lines_per_exec = 0;
	  break;

	case 'L':		/* POSIX */
	case 'l':		/* deprecated */
	  if (optarg)
	    bc_ctl.lines_per_exec = parse_num (optarg, 'l', 1L, -1L, 1);
	  else
	    bc_ctl.lines_per_exec = 1;
	  /* -l excludes -i -n.  */
	  bc_ctl.args_per_exec = 0;
	  bc_ctl.replace_pat = NULL;
	  break;

	case 'n':
	  bc_ctl.args_per_exec = parse_num (optarg, 'n', 1L, -1L, 1);
	  /* -n excludes -i -l.  */
	  bc_ctl.lines_per_exec = 0;
	  if (bc_ctl.args_per_exec == 1 && bc_ctl.replace_pat)
	    /* ignore -n1 in '-i -n1' */
	    bc_ctl.args_per_exec = 0;
	  else
	    bc_ctl.replace_pat = NULL;
	  break;

	  /* The POSIX standard specifies that it is not an error 
	   * for the -s option to specify a size that the implementation 
	   * cannot support - in that case, the relevant limit is used.
	   */
	case 's':
	  arg_size = parse_num (optarg, 's', 1L, posix_arg_size_max, 0);
	  if (arg_size > posix_arg_size_max)
	    {
	      error (0, 0, "warning: value %ld for -s option is too large, using %ld instead", arg_size, posix_arg_size_max);
	      arg_size = posix_arg_size_max;
	    }
	  break;

	case 'S':
	  show_limits = true;
	  break;

	case 't':
	  print_command = true;
	  break;

	case 'x':
	  bc_ctl.exit_if_size_exceeded = true;
	  break;

	case 'p':
	  query_before_executing = true;
	  print_command = true;
	  break;

	case 'r':
	  always_run_command = 0;
	  break;

	case 'P':
	  proc_max = parse_num (optarg, 'P', 0L, -1L, 1);
	  break;

        case 'a':
          input_file = optarg;
          break;

	case 'v':
	  printf (_("GNU xargs version %s\n"), version_string);
	  return 0;

	default:
	  usage (stderr);
	  return 1;
	}
    }

  if (0 == strcmp (input_file, "-"))
    {
      input_stream = stdin;
    }
  else
    {
      keep_stdin = 1;		/* see prep_child_for_exec() */
      input_stream = fopen (input_file, "r");
      if (NULL == input_stream)
	{
	  error (1, errno,
		 _("Cannot open input file `%s'"),
		 input_file);
	}
    }

  if (bc_ctl.replace_pat || bc_ctl.lines_per_exec)
    bc_ctl.exit_if_size_exceeded = true;

  if (optind == argc)
    {
      optind = 0;
      argc = 1;
      argv = &default_cmd;
    }

  /* Taking into account the size of the environment, 
   * figure out how large a buffer we need to
   * hold all the arguments.  We cannot use ARG_MAX 
   * directly since that may be arbitrarily large.
   * This is from a patch by Bob Prolux, <bob@proulx.com>.
   */
  if (bc_ctl.arg_max > arg_size)
    {
      if (show_limits)
	{
	  fprintf(stderr,
		  _("Reducing arg_max (%ld) to arg_size (%ld)\n"),
		  bc_ctl.arg_max, arg_size);
	}
      bc_ctl.arg_max = arg_size;
    }

  if (show_limits)
    {
      fprintf(stderr,
	      _("Your environment variables take up %ld bytes\n"),
	      size_of_environment);
      fprintf(stderr,
	      _("POSIX lower and upper limits on argument length: %ld, %ld\n"),
	      posix_arg_size_min,
	      posix_arg_size_max);
      fprintf(stderr,
	      _("Maximum length of command we could actually use: %ld\n"),
	      (posix_arg_size_max - size_of_environment));
      fprintf(stderr,
	      _("Size of command buffer we are actually using: %ld\n"),
	      arg_size);
    }
  
  linebuf = (char *) xmalloc (bc_ctl.arg_max + 1);
  bc_state.argbuf = (char *) xmalloc (bc_ctl.arg_max + 1);

  /* Make sure to listen for the kids.  */
  signal (SIGCHLD, SIG_DFL);

  if (!bc_ctl.replace_pat)
    {
      for (; optind < argc; optind++)
	bc_push_arg (&bc_ctl, &bc_state,
		     argv[optind], strlen (argv[optind]) + 1,
		     NULL, 0,
		     initial_args);
      initial_args = false;
      bc_ctl.initial_argc = bc_state.cmd_argc;
      bc_state.cmd_initial_argv_chars = bc_state.cmd_argv_chars;

      while ((*read_args) () != -1)
	if (bc_ctl.lines_per_exec && lineno >= bc_ctl.lines_per_exec)
	  {
	    xargs_do_exec (&bc_ctl, &bc_state);
	    lineno = 0;
	  }

      /* SYSV xargs seems to do at least one exec, even if the
         input is empty.  */
      if (bc_state.cmd_argc != bc_ctl.initial_argc
	  || (always_run_command && procs_executed == 0))
	xargs_do_exec (&bc_ctl, &bc_state);

    }
  else
    {
      int i;
      size_t len;
      size_t *arglen = (size_t *) xmalloc (sizeof (size_t) * argc);

      for (i = optind; i < argc; i++)
	arglen[i] = strlen(argv[i]);
      bc_ctl.rplen = strlen (bc_ctl.replace_pat);
      while ((len = (*read_args) ()) != -1)
	{
	  /* Don't do insert on the command name.  */
	  bc_clear_args(&bc_ctl, &bc_state);
	  bc_state.cmd_argv_chars = 0; /* begin at start of buffer */
	  
	  bc_push_arg (&bc_ctl, &bc_state,
		       argv[optind], arglen[optind] + 1,
		       NULL, 0,
		       initial_args);
	  len--;
	  initial_args = false;
	  
	  for (i = optind + 1; i < argc; i++)
	    bc_do_insert (&bc_ctl, &bc_state,
			  argv[i], arglen[i],
			  NULL, 0,
			  linebuf, len,
			  initial_args);
	  xargs_do_exec (&bc_ctl, &bc_state);
	}
    }

  return child_error;
}

#if 0
static int
append_char_to_buf(char **pbuf, char **pend, char **pp, int c)
{
  char *end_of_buffer = *pend;
  char *start_of_buffer = *pbuf;
  char *p = *pp;
  if (p >= end_of_buffer)
    {
      if (bc_ctl.replace_pat)
	{
	  size_t len = end_of_buffer - start_of_buffer;
	  size_t offset = p - start_of_buffer;
	  len *= 2;
	  start_of_buffer = xrealloc(start_of_buffer, len*2);
	  if (NULL != start_of_buffer)
	    {
	      end_of_buffer = start_of_buffer + len;
	      p = start_of_buffer + offset;
	      *p++ = c;

	      /* Update the caller's idea of where the buffer is. */
	      *pbuf = start_of_buffer;
	      *pend = end_of_buffer;
	      *pp = p;
	      
	      return 0;
	    }
	  else
	    {
	      /* Failed to reallocate. */
	      return -1;
	    }
	}
      else
	{
	  /* I suspect that this can never happen now, because append_char_to_buf()
	   * should only be called when replace_pat is true.
	   */
	  error (1, 0, _("argument line too long"));
	  /*NOTREACHED*/
	  return -1;
	}
    }
  else
    {
      /* Enough space remains. */
      *p++ = c;
      *pp = p;
      return 0;
    }
}
#endif


/* Read a line of arguments from the input and add them to the list of
   arguments to pass to the command.  Ignore blank lines and initial blanks.
   Single and double quotes and backslashes quote metacharacters and blanks
   as they do in the shell.
   Return -1 if eof (either physical or logical) is reached,
   otherwise the length of the last string read (including the null).  */

static int
read_line (void)
{
  static boolean eof = false;
  /* Start out in mode SPACE to always strip leading spaces (even with -i).  */
  int state = SPACE;		/* The type of character we last read.  */
  int prevc;			/* The previous value of c.  */
  int quotc = 0;		/* The last quote character read.  */
  int c = EOF;
  boolean first = true;		/* true if reading first arg on line.  */
  int len;
  char *p = linebuf;
  /* Including the NUL, the args must not grow past this point.  */
  char *endbuf = linebuf + bc_ctl.arg_max - bc_state.cmd_initial_argv_chars - 1;

  if (eof)
    return -1;
  while (1)
    {
      prevc = c;
      c = getc (input_stream);
      if (c == EOF)
	{
	  /* COMPAT: SYSV seems to ignore stuff on a line that
	     ends without a \n; we don't.  */
	  eof = true;
	  if (p == linebuf)
	    return -1;
	  *p++ = '\0';
	  len = p - linebuf;
	  if (state == QUOTE)
	    {
	      exec_if_possible ();
	      error (1, 0, _("unmatched %s quote; by default quotes are special to xargs unless you use the -0 option"),
		     quotc == '"' ? _("double") : _("single"));
	    }
	  if (first && EOF_STR (linebuf))
	    return -1;
	  if (!bc_ctl.replace_pat)
	    bc_push_arg (&bc_ctl, &bc_state,
			 linebuf, len,
			 NULL, 0,
			 initial_args);
	  return len;
	}
      switch (state)
	{
	case SPACE:
	  if (ISSPACE (c))
	    continue;
	  state = NORM;
	  /* aaahhhh....  */

	case NORM:
	  if (c == '\n')
	    {
	      if (!ISBLANK (prevc))
		lineno++;	/* For -l.  */
	      if (p == linebuf)
		{
		  /* Blank line.  */
		  state = SPACE;
		  continue;
		}
	      *p++ = '\0';
	      len = p - linebuf;
	      if (EOF_STR (linebuf))
		{
		  eof = true;
		  return first ? -1 : len;
		}
	      if (!bc_ctl.replace_pat)
		bc_push_arg (&bc_ctl, &bc_state,
			     linebuf, len,
			     NULL, 0,
			     initial_args);
	      return len;
	    }
	  if (!bc_ctl.replace_pat && ISSPACE (c))
	    {
	      *p++ = '\0';
	      len = p - linebuf;
	      if (EOF_STR (linebuf))
		{
		  eof = true;
		  return first ? -1 : len;
		}
	      bc_push_arg (&bc_ctl, &bc_state,
			   linebuf, len,
			   NULL, 0,
			   initial_args);
	      p = linebuf;
	      state = SPACE;
	      first = false;
	      continue;
	    }
	  switch (c)
	    {
	    case '\\':
	      state = BACKSLASH;
	      continue;

	    case '\'':
	    case '"':
	      state = QUOTE;
	      quotc = c;
	      continue;
	    }
	  break;

	case QUOTE:
	  if (c == '\n')
	    {
	      exec_if_possible ();
	      error (1, 0, _("unmatched %s quote; by default quotes are special to xargs unless you use the -0 option"),
		     quotc == '"' ? _("double") : _("single"));
	    }
	  if (c == quotc)
	    {
	      state = NORM;
	      continue;
	    }
	  break;

	case BACKSLASH:
	  state = NORM;
	  break;
	}
#if 1
      if (p >= endbuf)
        {
	  exec_if_possible ();
	  error (1, 0, _("argument line too long"));
	}
      *p++ = c;
#else
      append_char_to_buf(&linebuf, &endbuf, &p, c);
#endif
    }
}

/* Read a null-terminated string from the input and add it to the list of
   arguments to pass to the command.
   Return -1 if eof (either physical or logical) is reached,
   otherwise the length of the string read (including the null).  */

static int
read_string (void)
{
  static boolean eof = false;
  int len;
  char *p = linebuf;
  /* Including the NUL, the args must not grow past this point.  */
  char *endbuf = linebuf + bc_ctl.arg_max - bc_state.cmd_initial_argv_chars - 1;

  if (eof)
    return -1;
  while (1)
    {
      int c = getc (input_stream);
      if (c == EOF)
	{
	  eof = true;
	  if (p == linebuf)
	    return -1;
	  *p++ = '\0';
	  len = p - linebuf;
	  if (!bc_ctl.replace_pat)
	    bc_push_arg (&bc_ctl, &bc_state,
			 linebuf, len,
			 NULL, 0,
			 initial_args);
	  return len;
	}
      if (c == '\0')
	{
	  lineno++;		/* For -l.  */
	  *p++ = '\0';
	  len = p - linebuf;
	  if (!bc_ctl.replace_pat)
	    bc_push_arg (&bc_ctl, &bc_state,
			 linebuf, len,
			 NULL, 0,
			 initial_args);
	  return len;
	}
      if (p >= endbuf)
        {
	  exec_if_possible ();
	  error (1, 0, _("argument line too long"));
	}
      *p++ = c;
    }
}

/* Print the arguments of the command to execute.
   If ASK is nonzero, prompt the user for a response, and
   if the user responds affirmatively, return true;
   otherwise, return false.  */

static boolean
print_args (boolean ask)
{
  int i;

  for (i = 0; i < bc_state.cmd_argc - 1; i++)
    fprintf (stderr, "%s ", bc_state.cmd_argv[i]);
  if (ask)
    {
      static FILE *tty_stream;
      int c, savec;

      if (!tty_stream)
	{
	  tty_stream = fopen ("/dev/tty", "r");
	  if (!tty_stream)
	    error (1, errno, "/dev/tty");
	}
      fputs ("?...", stderr);
      fflush (stderr);
      c = savec = getc (tty_stream);
      while (c != EOF && c != '\n')
	c = getc (tty_stream);
      if (savec == 'y' || savec == 'Y')
	return true;
    }
  else
    putc ('\n', stderr);

  return false;
}


/* Close stdin and attach /dev/null to it.
 * This resolves Savannah bug #3992.
 */
static void
prep_child_for_exec (void)
{
  if (!keep_stdin)
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
}


/* Execute the command that has been built in `cmd_argv'.  This may involve
   waiting for processes that were previously executed.  */

static int
xargs_do_exec (const struct buildcmd_control *ctl, struct buildcmd_state *state)
{
  pid_t child;

  bc_push_arg (&bc_ctl, &bc_state,
	       (char *) NULL, 0,
	       NULL, 0,
	       false); /* Null terminate the arg list.  */
  
  if (!query_before_executing || print_args (true))
    {
      if (proc_max && procs_executing >= proc_max)
	wait_for_proc (false);
      if (!query_before_executing && print_command)
	print_args (false);
      /* If we run out of processes, wait for a child to return and
         try again.  */
      while ((child = fork ()) < 0 && errno == EAGAIN && procs_executing)
	wait_for_proc (false);
      switch (child)
	{
	case -1:
	  error (1, errno, _("cannot fork"));

	case 0:		/* Child.  */
	  prep_child_for_exec();
	  execvp (bc_state.cmd_argv[0], bc_state.cmd_argv);
	  error (0, errno, "%s", bc_state.cmd_argv[0]);
	  _exit (errno == ENOENT ? 127 : 126);
	  /*NOTREACHED*/
	}
      add_proc (child);
    }

  bc_clear_args(&bc_ctl, &bc_state);
  return 1;			/* Success */
}

/* Execute the command if possible.  */

static void
exec_if_possible (void)
{
  if (bc_ctl.replace_pat || initial_args ||
      bc_state.cmd_argc == bc_ctl.initial_argc || bc_ctl.exit_if_size_exceeded)
    return;
  xargs_do_exec (&bc_ctl, &bc_state);
}

/* Add the process with id PID to the list of processes that have
   been executed.  */

static void
add_proc (pid_t pid)
{
  int i;

  /* Find an empty slot.  */
  for (i = 0; i < pids_alloc && pids[i]; i++)
    ;
  if (i == pids_alloc)
    {
      if (pids_alloc == 0)
	{
	  pids_alloc = proc_max ? proc_max : 64;
	  pids = (pid_t *) xmalloc (sizeof (pid_t) * pids_alloc);
	}
      else
	{
	  pids_alloc *= 2;
	  pids = (pid_t *) xrealloc (pids,
				     sizeof (pid_t) * pids_alloc);
	}
      memset (&pids[i], '\0', sizeof (pid_t) * (pids_alloc - i));
    }
  pids[i] = pid;
  procs_executing++;
  procs_executed++;
}

/* If ALL is true, wait for all child processes to finish;
   otherwise, wait for one child process to finish.
   Remove the processes that finish from the list of executing processes.  */

static void
wait_for_proc (boolean all)
{
  while (procs_executing)
    {
      int i, status;

      do
	{
	  pid_t pid;

	  while ((pid = wait (&status)) == (pid_t) -1)
	    if (errno != EINTR)
	      error (1, errno, _("error waiting for child process"));

	  /* Find the entry in `pids' for the child process
	     that exited.  */
	  for (i = 0; i < pids_alloc && pid != pids[i]; i++)
	    ;
	}
      while (i == pids_alloc);	/* A child died that we didn't start? */

      /* Remove the child from the list.  */
      pids[i] = 0;
      procs_executing--;

      if (WEXITSTATUS (status) == 126 || WEXITSTATUS (status) == 127)
	exit (WEXITSTATUS (status));	/* Can't find or run the command.  */
      if (WEXITSTATUS (status) == 255)
	error (124, 0, _("%s: exited with status 255; aborting"), bc_state.cmd_argv[0]);
      if (WIFSTOPPED (status))
	error (125, 0, _("%s: stopped by signal %d"), bc_state.cmd_argv[0], WSTOPSIG (status));
      if (WIFSIGNALED (status))
	error (125, 0, _("%s: terminated by signal %d"), bc_state.cmd_argv[0], WTERMSIG (status));
      if (WEXITSTATUS (status) != 0)
	child_error = 123;

      if (!all)
	break;
    }
}

/* Wait for all child processes to finish.  */

static void
wait_for_proc_all (void)
{
  static boolean waiting = false;

  if (waiting)
    return;

  waiting = true;
  wait_for_proc (true);
  waiting = false;
}

/* Return the value of the number represented in STR.
   OPTION is the command line option to which STR is the argument.
   If the value does not fall within the boundaries MIN and MAX,
   Print an error message mentioning OPTION.  If FATAL is true, 
   we also exit. */

static long
parse_num (char *str, int option, long int min, long int max, int fatal)
{
  char *eptr;
  long val;

  val = strtol (str, &eptr, 10);
  if (eptr == str || *eptr)
    {
      fprintf (stderr, _("%s: invalid number for -%c option\n"),
	       program_name, option);
      usage (stderr);
      exit(1);
    }
  else if (val < min)
    {
      fprintf (stderr, _("%s: value for -%c option should be >= %ld\n"),
	       program_name, option, min);
      if (fatal)
	{
	  usage (stderr);
	  exit(1);
	}
      else
	{
	  val = min;
	}
    }
  else if (max >= 0 && val > max)
    {
      fprintf (stderr, _("%s: value for -%c option should be < %ld\n"),
	       program_name, option, max);
      if (fatal)
	{
	  usage (stderr);
	  exit(1);
	}
      else
	{
	  val = max;
	}
    }
  return val;
}

/* Return how much of ARG_MAX is used by the environment.  */

static long
env_size (char **envp)
{
  long len = 0;

  while (*envp)
    len += strlen (*envp++) + 1;

  return len;
}

static void
usage (FILE *stream)
{
  fprintf (stream, _("\
Usage: %s [-0prtx] [-e[eof-str]] [-i[replace-str]] [-l[max-lines]]\n\
       [-n max-args] [-s max-chars] [-P max-procs] [--null] [--eof[=eof-str]]\n\
       [--replace[=replace-str]] [--max-lines[=max-lines]] [--interactive]\n\
       [--max-chars=max-chars] [--verbose] [--exit] [--max-procs=max-procs]\n\
       [--max-args=max-args] [--no-run-if-empty] [--arg-file=file]\n\
       [--version] [--help] [command [initial-arguments]]\n"),
	   program_name);
  fputs (_("\nReport bugs to <bug-findutils@gnu.org>.\n"), stream);
}
