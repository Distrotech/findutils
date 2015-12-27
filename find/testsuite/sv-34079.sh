#! /bin/sh
# Copyright (C) 2011 Free Software Foundation, Inc.
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

# This test verifies that find does not have excessive memory consumption
# even for large directories.   It's not executed by default; it will only
# run if the environment variable RUN_VERY_EXPENSIVE_TESTS is set.

testname="$(basename $0)"

. "${srcdir}"/binary_locations.sh

make_test_data() {
    d="$1"
    (
	cd "$1" && echo "Creating test data in $(pwd -P)" >&2 || exit 1
	max=400
	for i in $(seq 0 400)
	do
	    printf "\r%03d/%03d" $i $max >&2
	    for j in $(seq 0 10000)
	    do
		printf "%03d_%04d " $i $j
	    done
	done | xargs sh -c 'touch "$@" || exit 255' fnord || exit 1
	printf "\rTest files created.\n" >&2
    )
}


if [ -n "${RUN_VERY_EXPENSIVE_TESTS}" ]; then
    if outdir=$(mktemp -d); then
	# Create some test files.
	bad=""
	printf "Generating test data in %s (this may take some time...):\n" \
	    "${outdir}" >&2
	if make_test_data "${outdir}"; then
	    # We don't check oldfind, as it uses savedir, meaning that
	    # it stores all the directory entries.  Hence the excessive
	    # memory consumption bug applies to oldfind even though it is
	    # not using fts.
	    for exe in "${ftsfind}" "${oldfind}"; do
	        echo "Checking memory consumption of ${exe}..." >&2
                if ( ulimit -v 50000 && ${exe} "${outdir}" >/dev/null; ); then
                        echo "Memory consumption of ${exe} is reasonable" >&2
                else
                        bad="${bad}${bad:+\n}Memory consumption of ${exe} is too high"
                fi
	    done
	else
	    bad="failed to set up the test in ${outdir}"
	fi
	rm -rf "${outdir}" || exit 1
	if [ -n "${bad}" ]; then
	    echo "${bad}" >&2
	    exit 1
	fi
    else
	echo "FAIL: could not create a test output file." >&2
	exit 1
    fi
else
    echo "${testname} was not run because" '${RUN_VERY_EXPENSIVE_TESTS}' \
	"is unset."
fi
