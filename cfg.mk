# cfg.mk -- configuration file for the maintainer makefile provided by gnulib.
# Copyright (C) 2010 Free Software Foundation, Inc.
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

local-checks-to-skip :=

# Errors I think are too picky anyway.
local-checks-to-skip += sc_error_message_period sc_error_message_uppercase \
	sc_file_system

# Errors I have not investigated; diagnose and fix later.
local-checks-to-skip += sc_makefile_at_at_check

# False positives I don't have a workaround for yet.
# sc_space_tab: several .xo test output files contain this sequence
#               for testing xargs's handling of white space.
local-checks-to-skip += sc_obsolete_symbols sc_prohibit_cvs_keyword \
	sc_two_space_separator_in_usage \
	sc_space_tab

# Problems that have some false positives and some real ones; tease
# apart later.
local-checks-to-skip += sc_trailing_blank

# Problems partly fixed in other patches which aren't merged yet.
local-checks-to-skip += sc_useless_cpp_parens

# Problems we can't esaily fixed because they apply to files which we need
# to keep in sync, so can't easily make a local change to.
# sc_texinfo_acronym: perms.texi from coreutils uses @acronym{GNU}.
local-checks-to-skip += \
	sc_texinfo_acronym

# sc_prohibit_strcmp is broken because it gives false positives for
# cases where neither argument is a string literal.
local-checks-to-skip += sc_prohibit_strcmp


# NEWS hash.  We use this to detect unintended edits to bits of the NEWS file
# other than the most recent section.   If you do need to retrospectively update
# a historic section, run "make update-NEWS-hash", which will then edit this file.
old_NEWS_hash := d41d8cd98f00b204e9800998ecf8427e
