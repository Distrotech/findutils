/* Save the list of files in a directory, with additional information.

   Copyright 1997, 1999, 2001, 2003, 2005 Free Software Foundation, Inc.

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

/* Written by James Youngman <jay@gnu.org>.
 * Based on savedir.h by David MacKenzie <djm@gnu.org>.
 */

#if !defined SAVEDIRINFO_H_
# define SAVEDIRINFO_H_

struct savedir_dirinfo
{
  mode_t type_info;
};

char *savedirinfo (const char *dir, struct savedir_dirinfo **extra);

#endif
