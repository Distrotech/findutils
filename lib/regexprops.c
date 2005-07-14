/* regexprops.c -- document the properties of the regular expressions 
                   understood by gnulib.

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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "regex.h"
#include "regextype.h"


static void output(const char *s, int escape)
{
  fputs(s, stdout);
}


static void newline(void)
{
  output("\n", 0);
}

static void content(const char *s)
{
  output(s, 1);
}

static void directive(const char *s)
{
  output(s, 0);
}

static void enum_item(const char *s)
{
  directive("@item ");
  content(s);
  newline();
}
static void table_item(const char *s)
{
  directive("@item");
  newline();
  content(s);
  newline();
}

static void code(const char *s)
{
  directive("@code{");
  content(s);
  directive("}");
}

static void begin_subsection(const char *name,
			  const char *next,
			  const char *prev,
			  const char *up)
{
  newline();
  
  directive("@node ");
  content(name);
  content(",");
  content(next);
  content(",");
  content(prev);
  content(",");
  content(up);
  newline();
  
  directive("@subsection ");
  content(name);
  newline();
}

static void begintable()
{
  newline();
  directive("@table @asis");
  newline();
}

static void endtable()
{
  newline();
  directive("@end table");
  newline();
}

static void beginenum()
{
  newline();
  directive("@enumerate");
  newline();
}

static void endenum()
{
  newline();
  directive("@end enumerate");
  newline();
}

static void newpara()
{
  content("\n\n");
}


static int describe_regex_syntax(int options)
{

  if (options & RE_NO_BK_PARENS)
    {
      content("Grouping is performed with parentheses ().  ");
      
      if (options & RE_UNMATCHED_RIGHT_PAREN_ORD)
	content("An unmatched ) matches just itself.  ");
    }
  else
    {
      content("Grouping is performed with backslashes followed by parentheses \\( \\).  ");
    }
  
  if (options & RE_NO_BK_REFS)
    {
      content("A backslash followed by a digit matches that digit.  ");
    }
  else
    {
      content("A backslash followed by a digit acts as a back-reference and matches the same thing as the previous grouped expression indicated by that number.  For example \\2 matches the second group expression.  The order of group expressions is determined by the position of their opening parenthesis '");
      if (options & RE_NO_BK_PARENS)
	  content("(");
      else
	content("\\(");
      content("'.  ");
    }
  

  newpara();
  if (!(options & RE_LIMITED_OPS))
    {
      if (options & RE_NO_BK_VBAR)
	content("The alternation operator is |.  ");
      else
	content("The alternation operator is \\|. ");
    }

  content("Bracket expressions are used to match ranges of characters.  ");
  content("Bracket expressions where the range is backward, for example [z-a], are ");
  if (options & RE_NO_EMPTY_RANGES)
    content("invalid");
  else
    content("ignored");
  content(".  ");
  
  if (options &  RE_BACKSLASH_ESCAPE_IN_LISTS)
    content("Within square brackets, '\\' can be used to quote "
	    "the following character.  ");
  else
    content("Within square brackets, '\\' is taken literally.  ");

  newpara();
  if (!(options & RE_LIMITED_OPS))
    {
      begintable();
      if (options & RE_BK_PLUS_QM)
	{
	  enum_item("\\+");
	  content("indicates that the regular expression should match one"
		  " or more occurrences of the previous atom or regexp.  ");
	  enum_item("\\?");
	  content("indicates that the regular expression should match zero"
		  " or one occurrence of the previous atom or regexp.  ");
	  enum_item("+ and ? ");
	  content("match themselves.  ");
	}
      else
	{
	  enum_item("+");
	  content("indicates that the regular expression should match one"
		  " or more occurrences of the previous atom or regexp.  ");
	  enum_item("?");
	  content("indicates that the regular expression should match zero"
		  " or one occurrence of the previous atom or regexp.  ");
	  enum_item("\\+");
	  content("matches a '+'");
	  enum_item("\\?");
	  content("matches a '?'.  ");
	}
      endtable();
    }
  
  newpara();
  if (options & RE_CHAR_CLASSES)
    content("Character classes are supported.  ");
  else
    content("Character classes are not not supported, so for example you would need to use [0-9] instead of [[:digit:]].  ");


  newpara();
  if (options & RE_CONTEXT_INDEP_ANCHORS)
    {
      content("The characters ^ and $ always represent the beginning and end of a string respectively, except within square brackets.  Within brackets, ^ can be used to invert the membership of the character class being specified.  ");
    }
  else
    {
      content("The character ^ only represents the beginning of a string when it appears:");
      beginenum();
      enum_item("\nAt the beginning of a regular expression");
      enum_item("After an open-group, signified by ");
      if (options & RE_NO_BK_PARENS)
	{
	  content("(");
	}
      else
	{
	  content("\\(");
	}
      newline();
      if (!(options & RE_LIMITED_OPS))
	{
	  if (options & RE_NEWLINE_ALT)
	    enum_item("After a newline");
	  
	  if (options & RE_NO_BK_VBAR )
	    enum_item("After the alternation operator |");
	  else
	    enum_item("After the alternation operator \\|");
	}
      endenum();

      newpara();
      content("The character $ only represents the end of a string when it appears:");
      beginenum();
      enum_item("At the end of a regular expression");
      enum_item("Before an close-group, signified by ");
      if (options & RE_NO_BK_PARENS)
	{
	  content(")");
	}
      else
	{
	  content("\\)");
	}
      if (!(options & RE_LIMITED_OPS))
	{
	  if (options & RE_NEWLINE_ALT)
	    enum_item("Before a newline");
	  
	  if (options & RE_NO_BK_VBAR)
	    enum_item("Before the alternation operator |");
	  else
	    enum_item("Before the alternation operator \\|");
	}
      endenum();
    }
  newpara();

  if (!(options & RE_LIMITED_OPS) )
    {
      if ((options & RE_CONTEXT_INDEP_OPS)
	  && !(options & RE_CONTEXT_INVALID_OPS))
	{
	  content("The characters *, + and ? are special anywhere in a regular expression.  ");
	}
      else
	{
	  if (options & RE_BK_PLUS_QM)
	    content("\\*, \\+ and \\? ");
	  else
	    content("*, + and ? ");
	  
	  if (options & RE_CONTEXT_INVALID_OPS)
	    {
	      content("are special at any point in a regular expression except the following places, where they are illegal:");
	    }
	  else
	    {
	      content("are special at any point in a regular expression except:");
	    }
	  
	  beginenum();
	  enum_item("At the beginning of a regular expression");
	  enum_item("After an open-group, signified by ");
	  if (options & RE_NO_BK_PARENS)
	    {
	      content("(");
	    }
	  else
	    {
	      content("\\(");
	    }
	  if (!(options & RE_LIMITED_OPS))
	    {
	      if (options & RE_NEWLINE_ALT)
		enum_item("After a newline");
	      
	      if (options & RE_NO_BK_VBAR)
		enum_item("After the alternation operator |");
	      else
		enum_item("After the alternation operator \\|");
	    }
	  endenum();
	}
    }
  
  newpara();
  content("The character '.' matches any single character");
  if ( (options & RE_DOT_NEWLINE)  == 0 )
    {
      content(" except newline");
    }
  if (options & RE_DOT_NOT_NULL)
    {
      if ( (options & RE_DOT_NEWLINE)  == 0 )
	content(" and");
      else
	content(" except");

      content(" the null character");
    }
  content(".  ");

  if (options & RE_HAT_LISTS_NOT_NEWLINE)
    {
      content("Non-matching lists [^.....] do not ever match newline.  ");
    }
  
  if (options & RE_INTERVALS) 
    {
      if (options & RE_NO_BK_BRACES)
	content("Intervals are specified by @{ and @}.  ");
      else
	content("Intervals are specified by \\@{ and \\@}.  ");
    }
  if (options & RE_INVALID_INTERVAL_ORD)
    {
      content("Invalid intervals are treated as literals, for example 'a@{1' is treated as 'a\\@{1'");
    }
  else
    {
      content("Invalid intervals such as a@{1z are not accepted.  ");
    }

  newpara();
  if (options & RE_NO_POSIX_BACKTRACKING)
    {
      content("Matching succeeds as soon as the whole pattern is matched, meaning that the result may not be the longest possible match.  ");
    }
  else
    {
      content("The longest possible match is returned; this applies to the regular expression as a whole and (subject to this constraint) to subexpressions within groups.  ");
    }

  newpara();
  if (options & RE_NO_GNU_OPS)
    {
      content("GNU extensions are not supported and so "
	      "\\w, \\W, \\<, \\>, \\b, \\B, \\`, and \\' "
	      "match "
	      "w, W, <, >, b, B, `, and ' respectively.  ");
    }
  else
    {
      content("GNU extensions are supported:");
      beginenum();
      enum_item("\\w matches a character within a word");
      enum_item("\\W matches a character which is not within a word");
      enum_item("\\< matches the beginning of a word");
      enum_item("\\> matches the end of a word");
      enum_item("\\b matches a word boundary");
      enum_item("\\B matches characters which are not a word boundaries");
      enum_item("\\` matches the beginning of the whole input");
      enum_item("\\' matches the end of the whole input");
      endenum();
    }
}



static int menu()
{
  int i, options;
  const char *name;
  
  output("@menu\n", 0);
  for (i=0;
       options = get_regex_type_flags(i),
	 name=get_regex_type_name(i);
       ++i)
    {
      output("* ", 0);
      output(name, 0);
      output("::", 0);
      newline();
    }
  output("@end menu\n", 0);
}


static int describe_all(const char *up)
{
  const char *name, *next, *previous;
  int options;
  int i;

  menu();
  
  previous = "";
  
  for (i=0;
       options = get_regex_type_flags(i),
	 name=get_regex_type_name(i);
       ++i)
    {
      next = get_regex_type_name(i+1);
      if (NULL == next)
	next = "";
      begin_subsection(name, next, previous, up);
      describe_regex_syntax(options);
      previous = name;
    }
}



int main (int argc, char *argv[])
{
  const char *up = "";
  
  if (argc > 1)
    up = argv[1];
  
  describe_all(up);
  return 0;
}
