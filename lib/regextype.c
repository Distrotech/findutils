/* regextype.c -- Decode the name of a regular expression syntax into am
                  option name.

   Copyright 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by James Youngman, <jay@gnu.org>. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <stdio.h>

#include "regextype.h"
#include "regex.h"
#include "quote.h"
#include "quotearg.h"
#include "xalloc.h"
#include "error.h"


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



struct tagRegexTypeMap
{
  char *name;
  int   option_val;
};
static struct tagRegexTypeMap regex_map[] = 
  {
   { "emacs", RE_SYNTAX_EMACS },
   { "posix-awk", RE_SYNTAX_POSIX_AWK },
   { "posix-basic", RE_SYNTAX_POSIX_BASIC },
   { "posix-egrep", RE_SYNTAX_POSIX_EGREP },
   { "posix-extended", RE_SYNTAX_POSIX_EXTENDED }
   /* new and undocumented entries below... */
   ,{ "posix-minimal-basic", RE_SYNTAX_POSIX_MINIMAL_BASIC }
  };
enum { N_REGEX_MAP_ENTRIES = sizeof(regex_map)/sizeof(regex_map[0]) };

int
get_regex_type(const char *s)
{
  unsigned i;
  size_t msglen;
  char *buf, *p;
  
  msglen = 0u;
  for (i=0u; i<N_REGEX_MAP_ENTRIES; ++i)
    {
      if (0 == strcmp(regex_map[i].name, s))
	return regex_map[i].option_val;
      else
	msglen += strlen(quote(regex_map[i].name)) + 2u;
    }

  /* We didn't find a match for the type of regular expression that the 
   * user indicated they wanted.  Tell them what the options are.
   */
  p = buf = xmalloc(1u + msglen);
  for (i=0u; i<N_REGEX_MAP_ENTRIES; ++i)
    {
      if (i > 0u)
	{
	  strcpy(p, ", ");
	  p += 2;
	}
      p += sprintf(p, "%s", quote(regex_map[i].name));
    }
  
  error(1, 0, _("Unknown regular expression type %s; valid types are %s."),
	quote(s),
	buf);
  /*NOTREACHED*/
  return -1;
}

  
