# Source this file, don't execute it.

if [[ -z "${testname}" ]]; then
    echo 'Please set $testname before sourcing binary_locations.sh.' >&2
    exit 1
fi

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
