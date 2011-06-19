#! /bin/sh
testname="$(basename $0)"

. "${srcdir}"/binary_locations.sh

goldenfile="${srcdir}/test_escapechars.golden"

if outfile=$(mktemp); then
    for executable in "$oldfind" "$ftsfind"
    do
        if "$executable" . -maxdepth 0 \
            -printf 'OCTAL1: \1\n' \
            -printf 'OCTAL2: \02\n' \
            -printf 'OCTAL3: \003\n' \
            -printf 'OCTAL4: \0044\n' \
            -printf 'OCTAL8: \0028\n' \
            -printf 'BEL: \a\n' \
            -printf 'CR: \r\n' \
            -printf 'FF: \f\n' \
            -printf 'TAB: \t\n' \
            -printf 'VTAB: \v\n' \
            -printf 'BS: \b\n' \
            -printf 'BACKSLASH: \\\n' \
            -printf 'UNKNOWN: \z\n' \
            >| "${outfile}" 2>/dev/null; then
            if cmp "${outfile}" "${goldenfile}"; then
                rm "${outfile}"
            else
                exec >&2
                echo "FAIL: $executable produced incorrect output:"
                od -c "${outfile}"
                echo "Expected output was:"
                od -c "${goldenfile}"
                exit 1
            fi
        else
            echo "FAIL: $executable returned $?" >&2
            exit 1
        fi
    done
else
    echo "FAIL: could not create a test output file." >&2
    exit 1
fi
