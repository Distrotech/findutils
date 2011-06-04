/* unused-result.h -- macros for ensuring callers don't ignore return values
   Copyright (C) 2010, 2011 Free Software Foundation, Inc.

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

/* Taken from coreutils' fts_.h */
#ifndef _UNUSED_RESULT_H
# define _UNUSED_RESULT_H 1

# ifndef __GNUC_PREREQ
#  if defined __GNUC__ && defined __GNUC_MINOR__
#   define __GNUC_PREREQ(maj, min) \
          ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#  else
#   define __GNUC_PREREQ(maj, min) 0
#  endif
# endif

# if __GNUC_PREREQ (3,4)
#  undef __attribute_warn_unused_result__
#  define __attribute_warn_unused_result__ \
    __attribute__ ((__warn_unused_result__))
# else
#  define __attribute_warn_unused_result__ /* empty */
# endif

#endif
