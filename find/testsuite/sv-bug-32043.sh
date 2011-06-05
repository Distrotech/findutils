#! /bin/sh
testname="$(basename $0)"

parent="$(cd .. && pwd)"
if [[ -f "${parent}/ftsfind" ]]; then
    ftsfind="${parent}/ftsfind"
    oldfind="${parent}/find"
elif [[ -f "${parent}/oldfind" ]]; then
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
	    if ! [[ "$result" = "$expected" ]]; then
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
