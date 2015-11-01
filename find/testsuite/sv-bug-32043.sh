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
if tstdir=$(mktemp -d); then
    touch "$tstdir/["
    expected="$tstdir/["
    for executable in "$oldfind" "$ftsfind"; do
	if result=$("$executable" "$tstdir" -name '[' -print); then
	    if ! [ "$result" = "$expected" ]; then
		echo "FAIL: $testname with $executable returned '$result' but '$expected' was expected" >&2
		exit 1
	    fi
	else
	    echo "FAIL: $executable returned $?" >&2
	    exit 1
	fi
    done
    rm -rf "$tstdir"
else
    echo "FAIL: could not create a test directory." >&2
    exit 1
fi
