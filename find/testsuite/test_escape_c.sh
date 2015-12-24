#! /bin/sh
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

testname="$(basename $0)"

. "${srcdir}"/binary_locations.sh

goldenfile="${srcdir}/test_escapechars.golden"
expected='hello^.^world'

for executable in "$oldfind" "$ftsfind"
do
    if result="$($executable . -maxdepth 0 \
	    -printf 'hello^\cthere' \
	    -exec printf %s {} \; \
	    -printf '^world\n' )"; then
        if [ "${result}" != "${expected}" ]; then
            exec >&2
            echo "$executable produced incorrect output:"
            echo "${result}"
            echo "Expected output was:"
            echo "${expected}"
            exit 1
        fi
    else
        echo "$executable returned $?" >&2
        exit 1
    fi
done
