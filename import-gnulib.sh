#! /bin/sh
#
# import-gnulib.sh -- imports a copy of gnulib into findutils
# Copyright (C) 2003,2004,2005,2006,2007,2009 Free Software Foundation, Inc.
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
# This script is intended to populate the "gnulib" directory
# with a subset of the gnulib code, as provided by "gnulib-tool".
#
# To use it, just run this script with the top-level sourec directory
# as your working directory.

# If CDPATH is set, it will sometimes print the name of the directory
# to which you have moved.  Unsetting CDPATH prevents this, as does
# prefixing it with ".".
unset CDPATH

## Defaults
# cvsdir=/doesnotexist
git_repo="git://git.savannah.gnu.org/gnulib.git"
configfile="./import-gnulib.config"
need_checkout=yes
gnulib_changed=false

# If $GIT_CLONE_DEPTH is not set, apply a default.
: ${GIT_CLONE_DEPTH:=4}


# Remember arguments for comments we inject into output files
original_cmd_line_args="$@"

usage() {
    cat >&2 <<EOF
usage: $0 [-d gnulib-directory]

The default behaviour is to check out the Gnulib code via anonymous
CVS (or update it if there is a version already checked out).  The
checkout or update is performed to the gnulib version indicated in
the configuration file $configfile.

If you wish to work with a different version of gnulib, use the -d option
to specify the directory containing the gnulib code.
EOF
}


do_checkout () {
    local gitdir="$1"
    echo checking out gnulib from GIT in $gitdir

    if [ -z "$gnulib_version" ] ; then
	echo "Error: There should be a gnulib_version setting in $configfile, but there is not." >&2
	exit 1
    fi


    if ! [ -d "$gitdir" ] ; then
	if mkdir "$gitdir" ; then
	echo "Created $gitdir"
	else
	echo "Failed to create $gitdir" >&2
	exit 1
	fi
    fi

    # Change directory unconditionally before issuing git commands, because
    # we're dealing with two git repositories; the gnulib one and the
    # findutils one.

    if ( cd $gitdir && test -d gnulib/.git ; ) ; then
      echo "Git repository was already initialised."
    else
      echo "Cloning the git repository..."
      ( cd $gitdir && git clone --depth="${GIT_CLONE_DEPTH}" "$git_repo" ; )
    fi

    if ( cd $gitdir/gnulib &&
	    git diff --name-only --exit-code "$gnulib_version" ; ) ; then
        # We are already at the correct version.
        # Nothing to do
        gnulib_changed=false
        echo "Already at gnulib version $gnulib_version; no change"
    else
        gnulib_changed=true
        set -x
        ( cd $gitdir/gnulib &&
	    git fetch origin &&
	    git checkout "$gnulib_version" ; )
        set +x
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


    if [ -d gnulib ]
    then
	echo "Warning: directory gnulib already exists." >&2
    else
	mkdir gnulib
    fi

    set -x
    if "$tool" --import --symlink --with-tests --dir=. --lib=libgnulib --source-base=gnulib/lib --m4-base=gnulib/m4 --local-dir=gnulib-local $modules
    then
	set +x
    else
	set +x
	echo "$tool failed, exiting." >&2
	exit 1
    fi

    # gnulib-tool does not remove broken symlinks leftover from previous runs;
    # this assumes GNU find, but should be a safe no-op if it is not
    find -L gnulib -lname '*' -delete 2>/dev/null || :
}

rehack() {
    printf "Updating the license of $1... "
    # Use cp to get the permissions right first
    cp -fp "$1" "$1".new
    sed -e  \
's/Free Software Foundation\([;,]\) either version [2]/Free Software Foundation\1 either version 3/' < "$1" > "$1".new
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

    cat > gnulib/Makefile.am <<EOF
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
    aclocal -I m4 -I gnulib/m4     &&
    autoheader                     &&
    autoconf                       &&
    automake --add-missing --copy
}


update_version_file() {
    local ver
    outfile="lib/gnulib-version.c"
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
Please see the README section in gnulib-git/gnulib/lib/git-merge-changelog.c

If you've read that and don't want to use it, just set the environment variable
DO_NOT_WANT_CHANGELOG_DRIVER.

Example:
  git config merge.merge-changelog.name 'GNU-style ChangeLog merge driver'
  git config merge.merge-changelog.driver /usr/local/bin/git-merge-changelog  %O %A %B
  echo 'ChangeLog    merge=merge-changelog' >> .gitattributes
"
    if [[ -z "$DO_NOT_WANT_CHANGELOG_DRIVER" ]] ; then
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
                  sed -e 's/[ 	].*//')"
	if [[ $? -eq 0 ]]; then
	    if ! [[ -x "$driver" ]]; then
		echo "ERROR: Merge driver $driver is not executable." >&2
		echo "ERROR: Please fix $config_file or install $driver" >&2
		# Always fatal - if configured, the merge driver should work.
		exit 1
	    else
		if [[ -f .gitattributes ]] ; then
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


move_cvsdir() {
    local cvs_git_root=":pserver:anonymous@pserver.git.sv.gnu.org:/gnulib.git"

    if test -d gnulib-cvs/gnulib/CVS
    then
      if test x"$(cat gnulib-cvs/gnulib/CVS/Root)" == x"$cvs_git_root"; then
          # We cannot use the git-cvspserver interface because
          # "update -D" doesn't work.
          echo "WARNING: Migrating from git-cvs-pserver to native git..." >&2
          savedir=gnulib-cvs.before-nativegit-migration
      else
          # The old CVS repository is not updated any more.
          echo "WARNING: Migrating from old CVS repository to native git" >&2
          savedir=gnulib-cvs.before-git-migration
      fi
      mv gnulib-cvs $savedir || exit 1
      echo "Please delete $savedir eventually"
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
    # to use, even if we don't want to know the CVS version.
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
	move_cvsdir
	do_checkout gnulib-git
	check_merge_driver
	gnulibdir=gnulib-git/gnulib
    else
	echo "Warning: using gnulib code which already exists in $gnulibdir" >&2
    fi

    ## If the config file changed since we last imported, or the gnulib
    ## code itself changed, we will need to re-run gnulib-tool.
    lastconfig="./gnulib/last.config"
    config_changed=false
    if "$gnulib_changed" ; then
	echo "The gnulib code changed, we need to re-import it."
    else
	if test -e "$lastconfig" ; then
	    if cmp "$lastconfig" "$configfile" ; then
		echo "Both gnulib and the import config are unchanged."
	    else
		echo "The gnulib import config was changed."
		echo "We need to re-run gnulib-tool."
		config_changed=true
	    fi
	else
	    echo "$lastconfig does not exist, we need to run gnulib-tool."
	    config_changed=true
	fi
    fi

    ## Invoke gnulib-tool to import the code.
    local tool="${gnulibdir}"/gnulib-tool
    if $gnulib_changed || $config_changed ; then
	if run_gnulib_tool "${tool}" &&
	    hack_gnulib_tool_output "${gnulibdir}" &&
	    refresh_output_files &&
	    update_licenses &&
	    update_version_file &&
	    record_config_change "$configfile" "$lastconfig"
	then
	    echo Done.
	else
	    echo FAILED >&2
	    exit 1
	fi
    else
	echo "No change to the gnulib code or configuration."
	echo "Therefore, no need to run gnulib-tool."
    fi
}

main "$@"
