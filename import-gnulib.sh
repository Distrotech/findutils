#! /bin/sh
#
# import-gnulib.sh -- imports a copy of gnulib into findutils
# Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009, 2010,
#               2011 Free Software Foundation, Inc.
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
##########################################################################
#
# This script is intended to populate the "gl" directory
# with a subset of the gnulib code, as provided by "gnulib-tool".
#
# To use it, just run this script with the top-level source directory
# as your working directory.

# If CDPATH is set, it will sometimes print the name of the directory
# to which you have moved.  Unsetting CDPATH prevents this, as does
# prefixing it with ".".
unset CDPATH

## Defaults
configfile="./import-gnulib.config"
need_checkout=yes
gldest=gl

# If $GIT_CLONE_DEPTH is not set, apply a default.
: ${GIT_CLONE_DEPTH:=4}


# Remember arguments for comments we inject into output files
original_cmd_line_args="$@"

usage() {
    cat >&2 <<EOF
usage: $0 [-d gnulib-directory] [-a]

The default behaviour is to check out the Gnulib code via anonymous
CVS (or update it if there is a version already checked out).  The
checkout or update is performed to the gnulib version indicated in
the configuration file $configfile.

If you wish to work with a different version of gnulib, use the -d option
to specify the directory containing the gnulib code.

The -a option skips the import of the gnulib code, and just generates
the output files (for example 'configure').
EOF
}

do_submodule () {
    local sm_name="$1"
    if test -f .gitmodules; then
        if git config --file \
	    .gitmodules "submodule.${sm_name}.url" >/dev/null; then
            # Submodule config in .gitmodules is already in place.
            # Copy the submodule config into .git.
            git submodule init || exit $?
            # Update the gnulib module.
            git submodule update || exit $?
        else
	    # .gitmodules should include gnulib.
	    cat >&2 <<EOF
The .gitmodules file is present, but does not list ${sm_name}.
This version of findutils expects it to be there.
Please report this as a bug to bug-findutils@gnu.org.
The .gitmodules file contains this:
EOF
	    cat .gitmodules >&2
	    exit 1
	fi
    else
	# findutils should have .gitmodules
	cat >&2 <<EOF
The .gitmodules file is missing.  This version of findutils expects it
to be there.  Please report this as a bug to bug-findutils@gnu.org.
EOF
	exit 1
    fi
}

run_gnulib_tool() {
    local tool="$1"
    if test -f "$tool"
    then
	true
    else
	echo "$tool does not exist, did you specify the right directory?" >&2
	exit 1
    fi

    if test -x "$tool"
    then
	true
    else
	echo "$tool is not executable" >&2
	exit 1
    fi


    if [ -d "${gldest}" ]
    then
	echo "Warning: directory ${gldest} already exists." >&2
    else
	mkdir "${gldest}"
    fi

    set -x
    if "$tool" --import --symlink --with-tests \
	--dir=. --lib=libgnulib \
	--source-base="${gldest}"/lib \
	--m4-base="${gldest}"/m4 --local-dir=gnulib-local $modules
    then
	set +x
    else
	set +x
	echo "$tool failed, exiting." >&2
	exit 1
    fi

    # gnulib-tool does not remove broken symlinks leftover from previous runs;
    # this assumes GNU find, but should be a safe no-op if it is not
    find -L "${gldest}" -lname '*' -delete 2>/dev/null || :
}

rehack() {
    printf "Updating the license of $1... "
    # Use cp to get the permissions right first
    cp -fp "$1" "$1".new
    sed -e  \
's/Free Software Foundation\([;,]\) either ''version [2]/Free Software Foundation\1 either version 3/' < "$1" > "$1".new
    if cmp "$1" "$1".new >/dev/null
    then
	echo "no change needed."
	rm -f "$1".new
    else
	rm -f "$1" && mv "$1".new "$1"
	echo "done."
    fi
}



copyhack() {
    src="$1"
    dst="$2"
    shift 2
    if test -d "$dst"
    then
	dst="$dst"/"$(basename $src)"
    fi
    cp -fp "$src" "$dst" && rehack "$dst"
}


update_licenses() {
    for f in $gpl3_update_files
    do
      rehack "$f" || exit
    done
}



hack_gnulib_tool_output() {
    local gnulibdir="${1}"
    for file in $extra_files; do
      case $file in
	*/mdate-sh | */texinfo.tex) dest=doc;;
	*) dest=build-aux;;
      esac
      copyhack "${gnulibdir}"/"$file" "$dest" || exit
    done

    cat > gl/Makefile.am <<EOF
# Copyright (C) 2004, 2009 Free Software Foundation, Inc.
#
# This file is free software, distributed under the terms of the GNU
# General Public License.  As a special exception to the GNU General
# Public License, this file may be distributed as part of a program
# that contains a configuration script generated by Automake, under
# the same distribution terms as the rest of that program.
#
# This file was generated by $0 $original_cmd_line_args.
#
SUBDIRS = lib
EXTRA_DIST = m4/gnulib-cache.m4
EOF
}


refresh_output_files() {
    autopoint -f &&
    aclocal -I m4 -I gl/m4     &&
    autoheader                     &&
    autoconf                       &&
    automake --add-missing --copy
}


update_version_file() {
    local gnulib_git_dir="$1"
    local ver
    local outfile="lib/gnulib-version.c"
    local gnulib_version="$( cd ${gnulib_git_dir} && git show-ref -s HEAD )"

    if [ -z "$gnulib_version" ] ; then
	ver="unknown (locally modified code; no version number available)"
    else
	ver="$gnulib_version"
    fi


    cat > "${outfile}".new <<EOF
/* This file is automatically generated by $0 and simply records which version of gnulib we used. */
const char * const gnulib_version = "$ver";
EOF
    if test -f "$outfile" ; then
	if diff "${outfile}".new "${outfile}" > /dev/null ; then
	    rm "${outfile}".new
	    return 0
	fi
    fi
    mv "${outfile}".new "${outfile}"
}


check_merge_driver() {
    local config_file=".git/config"
    fixmsg="

We recommend that you use a git merge driver for the ChangeLog file.
This simplifies the task of merging branches.
Please see the README section in gnulib/gnulib/lib/git-merge-changelog.c

If you've read that and don't want to use it, just set the environment variable
DO_NOT_WANT_CHANGELOG_DRIVER.

Example:
  git config merge.merge-changelog.name 'GNU-style ChangeLog merge driver'
  git config merge.merge-changelog.driver '/usr/local/bin/git-merge-changelog  %O %A %B'
  echo 'ChangeLog    merge=merge-changelog' >> .gitattributes
"
    if [ -z "$DO_NOT_WANT_CHANGELOG_DRIVER" ] ; then
	if git branch | egrep -q '\* *(master|rel-)'; then
	# We are on the master branch or a release branch.
	# Perhaps the user is simply building from git sources.
	# Issue our message as a warning rather than an error.
	    fatal=false
	    label="Warning"
	else
	    fatal=true
	    label="ERROR"
	fi
    else
	fatal=false
	label="Warning"
    fi
    if git config --get  merge.merge-changelog.name >/dev/null ; then
        driver="$(git config --get merge.merge-changelog.driver |
                  sed -e 's/[   ].*//')"
	if [ $? -eq 0 ]; then
	    if ! [ -x "$driver" ]; then
		echo "ERROR: Merge driver $driver is not executable." >&2
		echo "ERROR: Please fix $config_file or install $driver" >&2
		# Always fatal - if configured, the merge driver should work.
		exit 1
	    else
		if [ -f .gitattributes ] ; then
		    echo "The ChangeLog merge driver configuration seems OK."
		else
		    echo "$label"': you have no .gitattributes file' >&2
		    echo "$fixmsg" >&2
		    if $fatal; then
			exit 1
		    fi
		fi
	    fi
	else
	    echo "$label"': There is no driver specified in [merge "merge-changelog"] in' "$config_file" >&2
	    echo "$fixmsg" >&2
	    if $fatal; then
		exit 1
	    fi
	fi
    else
	echo "$label"': There is no name specified in [merge "merge-changelog"] in' "$config_file" >&2
	echo "$fixmsg" >&2
	if $fatal; then
	    exit 1
	fi
    fi
}


record_config_change() {
    # $1: name of the import-gnulib.config file
    # $2: name of the last.config file
    echo "Recording current config from $1 in $2"
    if ! cp  "$1" "$2"; then
	rm -f "$2"
	false
    fi
}


check_old_gnulib_dir_layout() {
    # We used to keep the gnulib git repo in ./gnulib-git/ and use
    # ./gnulib/ as the destination directory into which we installed
    # (part of) the gnulib code.  This changed and now ./gnulib/
    # contains the gnulib submodule and the destination directory is
    # ./gl/.  In other words, ./gnulib/ used to be the destination,
    # but now it is the source.
fixmsg="\
Findutils now manages the gnulib source code as a git submodule.

If you are still using the directory layout in which the git tree for
gnulib lives in ./gnulib-git/, please fix this and re-run import-gnulib.sh.
The fix is very simple; please delete both ./gnulib/ and ./gnulib-git.

This wasn't done automatically for you just in case you had any local changes.
"

    if test -d ./gl/; then
	# Looks like we're already in the new directory layout.
	true
    elif test -d ./gnulib-git/; then
	cat >&2 <<EOF
You still have a ./gnulib-git directory.

$fixmsg
EOF
	false
    else
	# No ./gl/ and no ./gnulib-git/.   If ./gnulib/ exists, it might
	# be either.   If there is no ./gnulib/ we are safe to proceed anyway.
	if test -d ./gnulib/; then
	    if test -d ./gnulib/.git; then
		# Looks like it is the submodule.
		true
	    else
	cat >&2 <<EOF
You have a ./gnulib directory which does not appear to be a submodule.

$fixmsg
EOF
	false
	    fi
	else
	    # No ./gl/, no ./gnulib/, no ./gnulib-git/.
	    # It is safe to proceed anyway.
	    true
	fi
    fi
}

main() {
    ## Option parsing
    local gnulibdir=/doesnotexist
    while getopts "d:a" opt
    do
      case "$opt" in
	  d)  gnulibdir="$OPTARG" ; need_checkout=no ;;
	  a)  refresh_output_files && update_licenses ; exit $? ;;
	  **) usage; exit 1;;
      esac
    done

    # We need the config file to tell us which modules
    # to use, even if we don't want to know the gnulib version.
    . $configfile || exit 1

    ## If -d was not given, do update
    if [ $need_checkout = yes ] ; then
	if ! git version > /dev/null; then
	    cat >&2 <<EOF
You now need the tool 'git' in order to check out the correct version
of the gnulib code.  See http://git.or.cz/ for more information about git.
EOF
	    exit 1
	fi

        # Check for the old directory layout.
	echo "Checking the submodule directory layout... "
        check_old_gnulib_dir_layout || exit 1
	echo "The submodule directory layout looks OK."

	do_submodule gnulib
	check_merge_driver
	gnulibdir=gnulib
    else
	echo "Warning: using gnulib code which already exists in $gnulibdir" >&2
    fi

    local tool="${gnulibdir}"/gnulib-tool
    if run_gnulib_tool "${tool}" &&
        hack_gnulib_tool_output "${gnulibdir}" &&
        refresh_output_files &&
        update_licenses &&
        update_version_file "${gnulibdir}"
    then
        echo Done.
    else
        echo FAILED >&2
        exit 1
    fi
}

main "$@"
