#! /bin/sh
#
# Generate regexprops.texi and compare it against the checked-in version.
#
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
