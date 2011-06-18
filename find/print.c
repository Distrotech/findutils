/* print.c -- print/printf-related code.
   Copyright (C) 1990, 1991, 1992, 1993, 1994, 2000, 2001, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
   Inc.

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

/* We always include config.h first. */
#include <config.h>

/* system headers go here. */
#include <assert.h>
#include <ctype.h>
#include <locale.h>

/* gnulib headers. */
#include "error.h"
#include "xalloc.h"

/* find-specific headers. */
#include "defs.h"
#include "print.h"

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

#if defined STDC_HEADERS
# define ISDIGIT(c) isdigit ((unsigned char)c)
#else
# define ISDIGIT(c) (isascii ((unsigned char)c) && isdigit ((unsigned char)c))
#endif

/* Create a new fprintf segment in *SEGMENT, with type KIND,
   from the text in FORMAT, which has length LEN.
   Return the address of the `next' pointer of the new segment. */
struct segment **
make_segment (struct segment **segment,
	      char *format,
	      int len,
	      int kind,
	      char format_char,
	      char aux_format_char,
	      struct predicate *pred)
{
  enum EvaluationCost mycost = NeedsNothing;
  char *fmt;

  *segment = xmalloc (sizeof (struct segment));

  (*segment)->segkind = kind;
  (*segment)->format_char[0] = format_char;
  (*segment)->format_char[1] = aux_format_char;
  (*segment)->next = NULL;
  (*segment)->text_len = len;

  fmt = (*segment)->text = xmalloc (len + sizeof "d");
  strncpy (fmt, format, len);
  fmt += len;

  if (kind == KIND_PLAIN     /* Plain text string, no % conversion. */
      || kind == KIND_STOP)  /* Terminate argument, no newline. */
    {
      assert (0 == format_char);
      assert (0 == aux_format_char);
      *fmt = '\0';
      if (mycost > pred->p_cost)
	pred->p_cost = NeedsNothing;
      return &(*segment)->next;
    }

  assert (kind == KIND_FORMAT);
  switch (format_char)
    {
    case 'l':			/* object of symlink */
      pred->need_stat = true;
      mycost = NeedsLinkName;
      *fmt++ = 's';
      break;

    case 'y':			/* file type */
      pred->need_type = true;
      mycost = NeedsType;
      *fmt++ = 's';
      break;

    case 'i':			/* inode number */
      pred->need_inum = true;
      mycost = NeedsInodeNumber;
      *fmt++ = 's';
      break;

    case 'a':			/* atime in `ctime' format */
    case 'A':			/* atime in user-specified strftime format */
    case 'B':			/* birth time in user-specified strftime format */
    case 'c':			/* ctime in `ctime' format */
    case 'C':			/* ctime in user-specified strftime format */
    case 'F':			/* file system type */
    case 'g':			/* group name */
    case 'M':			/* mode in `ls -l' format (eg., "drwxr-xr-x") */
    case 's':			/* size in bytes */
    case 't':			/* mtime in `ctime' format */
    case 'T':			/* mtime in user-specified strftime format */
    case 'u':			/* user name */
      pred->need_stat = true;
      mycost = NeedsStatInfo;
      *fmt++ = 's';
      break;

    case 'S':			/* sparseness */
      pred->need_stat = true;
      mycost = NeedsStatInfo;
      *fmt++ = 'g';
      break;

    case 'Y':			/* symlink pointed file type */
      pred->need_stat = true;
      mycost = NeedsType;	/* true for amortised effect */
      *fmt++ = 's';
      break;

    case 'f':			/* basename of path */
    case 'h':			/* leading directories part of path */
    case 'p':			/* pathname */
    case 'P':			/* pathname with ARGV element stripped */
      *fmt++ = 's';
      break;

    case 'Z':			/* SELinux security context */
      mycost = NeedsAccessInfo;
      *fmt++ = 's';
      break;

    case 'H':			/* ARGV element file was found under */
      *fmt++ = 's';
      break;

      /* Numeric items that one might expect to honour
       * #, 0, + flags but which do not.
       */
    case 'G':			/* GID number */
    case 'U':			/* UID number */
    case 'b':			/* size in 512-byte blocks (NOT birthtime in ctime fmt)*/
    case 'D':                   /* Filesystem device on which the file exits */
    case 'k':			/* size in 1K blocks */
    case 'n':			/* number of links */
      pred->need_stat = true;
      mycost = NeedsStatInfo;
      *fmt++ = 's';
      break;

      /* Numeric items that DO honour #, 0, + flags.
       */
    case 'd':			/* depth in search tree (0 = ARGV element) */
      *fmt++ = 'd';
      break;

    case 'm':			/* mode as octal number (perms only) */
      *fmt++ = 'o';
      pred->need_stat = true;
      mycost = NeedsStatInfo;
      break;

    case '{':
    case '[':
    case '(':
      error (EXIT_FAILURE, 0,
	     _("error: the format directive `%%%c' is reserved for future use"),
	     (int)kind);
      /*NOTREACHED*/
      break;
    }
  *fmt = '\0';

  if (mycost > pred->p_cost)
    pred->p_cost = mycost;
  return &(*segment)->next;
}

bool
insert_fprintf (struct format_val *vec,
		const struct parser_table *entry,
		const char *format_const)
{
  char *format = (char*)format_const; /* XXX: casting away constness */
  register char *scan;		/* Current address in scanning `format'. */
  register char *scan2;		/* Address inside of element being scanned. */
  struct segment **segmentp;	/* Address of current segment. */
  struct predicate *our_pred;

  our_pred = insert_primary_withpred (entry, pred_fprintf, format_const);
  our_pred->side_effects = our_pred->no_default_print = true;
  our_pred->args.printf_vec = *vec;
  our_pred->need_type = false;
  our_pred->need_stat = false;
  our_pred->p_cost    = NeedsNothing;

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
		  make_segment (segmentp, format, scan - format,
				KIND_STOP, 0, 0,
				our_pred);
		  if (our_pred->need_stat && (our_pred->p_cost < NeedsStatInfo))
		    our_pred->p_cost = NeedsStatInfo;
		  return true;
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
				   KIND_PLAIN, 0, 0,
				   our_pred);
	  format = scan2 + 1;	/* Move past the escape. */
	  scan = scan2;		/* Incremented immediately by `for'. */
	}
      else if (*scan == '%')
	{
	  if (scan[1] == 0)
	    {
	      /* Trailing %.  We don't like those. */
	      error (EXIT_FAILURE, 0,
		     _("error: %s at end of format string"), scan);
	    }
	  else if (scan[1] == '%')
	    {
	      segmentp = make_segment (segmentp, format, scan - format + 1,
				       KIND_PLAIN, 0, 0,
				       our_pred);
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
	  if (strchr ("abcdDfFgGhHiklmMnpPsStuUyYZ", *scan2))
	    {
	      segmentp = make_segment (segmentp, format, scan2 - format,
				       KIND_FORMAT, *scan2, 0,
				       our_pred);
	      scan = scan2;
	      format = scan + 1;
	    }
	  else if (strchr ("ABCT", *scan2) && scan2[1])
	    {
	      segmentp = make_segment (segmentp, format, scan2 - format,
				       KIND_FORMAT, scan2[0], scan2[1],
				       our_pred);
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
				       KIND_PLAIN, 0, 0,
				       our_pred);
	      format = scan + 1;
	      continue;
	    }
	}
    }

  if (scan > format)
    make_segment (segmentp, format, scan - format, KIND_PLAIN, 0, 0,
		  our_pred);
  return true;
}
