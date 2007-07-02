/* strspn.c -- return numbers of chars at start of string in a class
   Copyright (C) 1987, 1990, 2004 Free Software Foundation, Inc.

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

#include <config.h>


#if defined HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#ifndef strchr
#define strchr index
#endif
#endif

#if !defined(HAVE_STRSPN)
int
strspn (str, class)
     char *str, *class;
{
  register char *st = str;

  while (*st && strchr (class, *st))
    ++st;
  return st - str;
}
#endif
