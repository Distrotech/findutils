/* buildcmd.c -- build command lines from a list of arguments.
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.
*/

#include <config.h>

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#include <string.h>
#endif


#if DO_MULTIBYTE
# if HAVE_MBRLEN
#  include <wchar.h>
# else
   /* Simulate mbrlen with mblen as best we can.  */
#  define mbstate_t int
#  define mbrlen(s, n, ps) mblen (s, n)
# endif
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

#ifndef _POSIX_SOURCE
#include <sys/param.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_UNISTD_H
/* for sysconf() */
#include <unistd.h>
#endif

/* COMPAT:  SYSV version defaults size (and has a max value of) to 470.
   We try to make it as large as possible. */
#if !defined(ARG_MAX) && defined(_SC_ARG_MAX)
#define ARG_MAX sysconf (_SC_ARG_MAX)
#endif
#ifndef ARG_MAX
#define ARG_MAX NCARGS
#endif



#include <xalloc.h>
#include <error.h>

#include "buildcmd.h"

static char *mbstrstr PARAMS ((const char *haystack, const char *needle));

/* Replace all instances of `replace_pat' in ARG with `linebuf',
   and add the resulting string to the list of arguments for the command
   to execute.
   ARGLEN is the length of ARG, not including the null.
   LBLEN is the length of LINEBUF, not including the null.
   PFXLEN is the length of PREFIX.  Substitution is not performed on
   the prefix.   The prefix is used if the argument contains replace_pat.

   COMPAT: insertions on the SYSV version are limited to 255 chars per line,
   and a max of 5 occurrences of replace_pat in the initial-arguments.
   Those restrictions do not exist here.  */

void
bc_do_insert (const struct buildcmd_control *ctl,
	      struct buildcmd_state *state,
	      char *arg, size_t arglen,
	      const char *prefix, size_t pfxlen,
	      const char *linebuf, size_t lblen,
	      int initial_args)
{
  /* Temporary copy of each arg with the replace pattern replaced by the
     real arg.  */
  static char *insertbuf;
  char *p;
  int bytes_left = ctl->arg_max - 1;	/* Bytes left on the command line.  */
  int need_prefix;
  
  if (!insertbuf)
    insertbuf = (char *) xmalloc (ctl->arg_max + 1);
  p = insertbuf;

  need_prefix = 0;
  do
    {
      size_t len;		/* Length in ARG before `replace_pat'.  */
      char *s = mbstrstr (arg, ctl->replace_pat);
      if (s)
	{
	  need_prefix = 1;
	  len = s - arg;
	}
      else
	{
	  len = arglen;
	}
      
      bytes_left -= len;
      if (bytes_left <= 0)
	break;

      strncpy (p, arg, len);
      p += len;
      arg += len;
      arglen -= len;

      if (s)
	{
	  bytes_left -= lblen;
	  if (bytes_left <= 0)
	    break;
	  strcpy (p, linebuf);
	  arg += ctl->rplen;
	  arglen -= ctl->rplen;
	  p += lblen;
	}
    }
  while (*arg);
  if (*arg)
    error (1, 0, _("command too long"));
  *p++ = '\0';

  if (!need_prefix)
    {
      prefix = NULL;
      pfxlen = 0;
    }
  
  bc_push_arg (ctl, state,
	       insertbuf, p - insertbuf,
	       prefix, pfxlen,
	       initial_args);
}

static
void do_exec(const struct buildcmd_control *ctl,
	     struct buildcmd_state *state)
{
  (ctl->exec_callback)(ctl, state);
}


/* Add ARG to the end of the list of arguments `cmd_argv' to pass
   to the command.
   LEN is the length of ARG, including the terminating null.
   If this brings the list up to its maximum size, execute the command.  */

void
bc_push_arg (const struct buildcmd_control *ctl,
	     struct buildcmd_state *state,
	     const char *arg, size_t len,
	     const char *prefix, size_t pfxlen,
	     int initial_args)
{
  state->todo = 1;
  if (arg)
    {
      if (state->cmd_argv_chars + len > ctl->arg_max)
	{
	  if (initial_args || state->cmd_argc == ctl->initial_argc)
	    error (1, 0, _("can not fit single argument within argument list size limit"));
	  /* option -i (replace_pat) implies -x (exit_if_size_exceeded) */
	  if (ctl->replace_pat
	      || (ctl->exit_if_size_exceeded &&
		  (ctl->lines_per_exec || ctl->args_per_exec)))
	    error (1, 0, _("argument list too long"));
	  do_exec (ctl, state);
	}
      if (!initial_args && ctl->args_per_exec &&
	  state->cmd_argc - ctl->initial_argc == ctl->args_per_exec)
	do_exec (ctl, state);
    }

  if (state->cmd_argc >= state->cmd_argv_alloc)
    {
      if (!state->cmd_argv)
	{
	  state->cmd_argv_alloc = 64;
	  state->cmd_argv = (char **) xmalloc (sizeof (char *) * state->cmd_argv_alloc);
	}
      else
	{
	  state->cmd_argv_alloc *= 2;
	  state->cmd_argv = (char **) xrealloc (state->cmd_argv,
					 sizeof (char *) * state->cmd_argv_alloc);
	}
    }

  if (!arg)
    state->cmd_argv[state->cmd_argc++] = NULL;
  else
    {
      state->cmd_argv[state->cmd_argc++] = state->argbuf + state->cmd_argv_chars;
      if (prefix)
	{
	  strcpy (state->argbuf + state->cmd_argv_chars, prefix);
	  state->cmd_argv_chars += pfxlen;
	}
      
      strcpy (state->argbuf + state->cmd_argv_chars, arg);
      state->cmd_argv_chars += len;
      
      /* If we have now collected enough arguments,
       * do the exec immediately.  This must be 
       * conditional on arg!=NULL, since do_exec() 
       * actually calls bc_push_arg(ctl, state, NULL, 0, false).
       */
      if (!initial_args
	  && ctl->args_per_exec
	  && (state->cmd_argc - ctl->initial_argc) == ctl->args_per_exec)
	do_exec (ctl, state);
    }

  /* If this is an initial argument, set the high-water mark. */
  if (initial_args)
    {
      state->cmd_initial_argv_chars = state->cmd_argv_chars;
    }
}


/* Finds the first occurrence of the substring NEEDLE in the string
   HAYSTACK.  Both strings can be multibyte strings.  */

static char *
mbstrstr (const char *haystack, const char *needle)
{
#if DO_MULTIBYTE
  if (MB_CUR_MAX > 1)
    {
      size_t hlen = strlen (haystack);
      size_t nlen = strlen (needle);
      mbstate_t mbstate;
      size_t step;

      memset (&mbstate, 0, sizeof (mbstate_t));
      while (hlen >= nlen)
	{
	  if (memcmp (haystack, needle, nlen) == 0)
	    return (char *) haystack;
	  step = mbrlen (haystack, hlen, &mbstate);
	  if (step <= 0)
	    break;
	  haystack += step;
	  hlen -= step;
	}
      return NULL;
    }
#endif
  return strstr (haystack, needle);
}


long
bc_get_arg_max(void)
{
  long val;
#ifdef _SC_ARG_MAX  
  val = sysconf(_SC_ARG_MAX);
#else
  val = -1;
#endif
  
  if (val > 0)
    return val;

  /* either _SC_ARG_MAX was not available or 
   * there is no particular limit.
   */
#ifdef ARG_MAX
  val = ARG_MAX;
#endif

  if (val > 0)
    return val;
  
  /* The value returned by this function bounds the
   * value applied as the ceiling for the -s option.
   * Hence it the system won't tell us what its limit
   * is, we allow the user to specify more or less 
   * whatever value they like.
   */
  return LONG_MAX;
}


static int cb_exec_noop(const struct buildcmd_control *ctl,
			 struct buildcmd_state *state)
{
  /* does nothing. */
  (void) ctl;
  (void) state;

  return 0;
}

void
bc_init_controlinfo(struct buildcmd_control *ctl)
{
  ctl->exit_if_size_exceeded = 0;
  ctl->arg_max = bc_get_arg_max() - 2048; /* a la xargs */
  ctl->rplen = 0u;
  ctl->replace_pat = NULL;
  ctl->initial_argc = 0;
  ctl->exec_callback = cb_exec_noop;
  ctl->lines_per_exec = 0;
  ctl->args_per_exec = 0;
}

void
bc_init_state(const struct buildcmd_control *ctl,
	      struct buildcmd_state *state,
	      void *context)
{
  state->cmd_argc = 0;
  state->cmd_argv_chars = 0;
  state->cmd_argv = NULL;
  state->cmd_argv_alloc = 0;
  state->argbuf = (char *) xmalloc (ctl->arg_max + 1);
  state->cmd_argv_chars = state->cmd_initial_argv_chars = 0;
  state->todo = 0;
  state->usercontext = context;
}

void 
bc_clear_args(const struct buildcmd_control *ctl,
	      struct buildcmd_state *state)
{
  state->cmd_argc = ctl->initial_argc;
  state->cmd_argv_chars = state->cmd_initial_argv_chars;
  state->todo = 0;
}

