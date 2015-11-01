# Source this file, don't execute it.
#
# Copyright (C) 2011-2015 Free Software Foundation, Inc.
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
#

if [ -z "${testname}" ]; then
    echo 'Please set $testname before sourcing binary_locations.sh.' >&2
    exit 1
fi

parent="$(cd .. && pwd)"
if [ -f "${parent}/ftsfind" ]; then
    ftsfind="${parent}/ftsfind"
    oldfind="${parent}/find"
elif [ -f "${parent}/oldfind" ]; then
    ftsfind="${parent}/find"
    oldfind="${parent}/oldfind"
else
    echo "Cannot find the executables to test." >&2
    exit 1
fi
