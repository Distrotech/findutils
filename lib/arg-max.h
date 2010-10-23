/* arg-max.h -- ARG_MAX and _SC_ARG_MAX checks and manipulations

   Copyright (C) 2010 Free Software Foundation, Inc.

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

   Written by James Youngman.
*/
#ifndef INC_ARG_MAX_H
#define INC_ARG_MAX_H 1

#include <unistd.h>		/* for sysconf() */

#ifdef __INTERIX
/* On Interix, _SC_ARG_MAX yields a value that (according to
 * http://lists.gnu.org/archive/html/bug-findutils/2010-10/msg00047.html)
 * actually does not work (i.e. it is larger than the real limit).
 * The value of ARG_MAX is reportedly smaller than the real limit.
 *
 * I considered a configure test for this which allocates an argument
 * list longer than ARG_MAX but shorter than _SC_ARG_MAX and then
 * tries to exec something, but this will not work for us when
 * cross-compiling if the target is Interix.
 *
 * Although buildcmd has heuristics for dealing with the possibility
 * that execve fails due to length limits in the implementation, it
 * assumed that changes are only necessary if execve fails with errno
 * set to E2BIG.  On Interix, this failure mode of execve appears to
 * set errno to ENOMEM.
 *
 * Since we may undefine _SC_ARG_MAX, we must include this header after
 * unistd.h.
 */
# undef _SC_ARG_MAX
# define BC_SC_ARG_MAX_IS_UNRELIABLE 1
#endif

#endif
