#! /bin/sh
# Copyright (C) 2007-2015 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

rv=0
srcdir="$1" ; shift

for manpage
do
  what="lint check on manpage $manpage"
  echo -n "$what: "
  messages="$( troff -t -man ${srcdir}/${manpage} 2>&1 >/dev/null )"
  if test -z "$messages" ; then
      echo "passed"
  else
      echo "FAILED:" >&2
      echo "$messages"     >&2
      rv=1
  fi
done
exit $rv
