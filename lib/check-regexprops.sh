#! /bin/sh
# Generate regexprops.texi and compare it against the checked-in version.
#
# Copyright (C) 2009-2015 Free Software Foundation, Inc.
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
existing=${srcdir}/../doc/regexprops.texi

case ${REGEXPROPS} in
    /*) ;;
    *) REGEXPROPS="./${REGEXPROPS}"
esac

save_output="regexprops.texi.new"

rv=1
if output_file=`mktemp ${TMPDIR:-/tmp}/check-regexprops.XXXXXX`
then
    ${REGEXPROPS} "Regular Expressions" findutils |
    sed -e 's/[     ][      ]*$//' >| "${output_file}"
    if cmp "${existing}" "${output_file}" ; then
	echo "${existing} is up to date."
	rv=0
    else
	echo "${existing} is out of date." >&2
	cp "${output_file}" "${save_output}" &&
	  echo "Updated output is saved in ${save_output}" >&2
	rv=1
    fi
    rm -f "${output_file}"
fi
exit "$rv"
