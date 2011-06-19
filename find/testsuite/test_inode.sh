#! /bin/sh
#
# Essentially this test verifies that ls -i and find -printf %i produce
# the same output.

testname="$(basename $0)"

. "${srcdir}"/binary_locations.sh

make_canonical() {
    sed -e 's/ /_/g'
}

test_percent_i() {
    if "${executable}" "${tmpfile}" -printf '%i_%p\n' |
	make_canonical >| "${outfile}"; then
	cmp "${outfile}" "${goldfile}" || {
	    exec >&2
	    cat <<EOF
${executable} ${printf_format} produced incorrect output.
Actual output:
$(cat ${outfile})
Expected output:
$(cat ${goldfile})
EOF
	    rm -f "${outfile}" "${goldfile}" "${tmpfile}"
	    exit 1
	}
    fi
}



if tmpfile=$(mktemp); then
    if goldfile=$(mktemp); then
	ls -i "${tmpfile}" | make_canonical >| "${goldfile}"

	if outfile=$(mktemp); then
	    for executable in "${oldfind}" "${ftsfind}"
	    do
		test_percent_i
	    done
	    rm -f "${outfile}"
	fi
	rm -f "${goldfile}"
    fi
    rm -f "${tmpfile}"
fi



