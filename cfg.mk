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

exclude_file_name_regexp--sc_obsolete_symbols = build-aux/src-sniff\.py
exclude_file_name_regexp--sc_space_tab = \
	xargs/testsuite/(inputs/.*\.xi|xargs.(gnu|posix|sysv)/.*\.xo)|find/testsuite/test_escapechars\.golden$$

# Skip sc_two_space_separator_in_usage because it reflects the requirements
# of help2man.   It gets run on files that are not help2man inputs, and in
# any case we don't use help2man at all.
local-checks-to-skip += sc_two_space_separator_in_usage

# Some test inputs/outputs have trailing blanks.
exclude_file_name_regexp--sc_trailing_blank = \
 ^COPYING|(po/.*\.po)|(find/testsuite/(test_escapechars\.golden|find.gnu/printf\.xo))|(xargs/testsuite/(inputs/.*\.xi|xargs\.(gnu|posix|sysv)/.*\.(x[oe])))$$

exclude_file_name_regexp--sc_prohibit_empty_lines_at_EOF = \
	^(.*/testsuite/.*\.(xo|xi|xe))|COPYING|doc/regexprops\.texi|m4/order-(bad|good)\.bin$$
exclude_file_name_regexp--sc_bindtextdomain = \
	^lib/(regexprops|test_splitstring)\.c$$
exclude_file_name_regexp--sc_prohibit_always_true_header_tests = \
	^(build-aux/src-sniff\.py)|ChangeLog$$
exclude_file_name_regexp--sc_prohibit_test_minus_ao = \
	^(ChangeLog)|((find|locate|xargs)/testsuite/.*\.exp)$$
exclude_file_name_regexp--sc_prohibit_doubled_word = \
	^(xargs/testsuite/xargs\.sysv/iquotes\.xo)|ChangeLog|po/.*\.po$$
exclude_file_name_regexp--sc_program_name = \
	^lib/test_splitstring\.c$$

# sc_texinfo_acronym: perms.texi from coreutils uses @acronym{GNU}.
exclude_file_name_regexp--sc_texinfo_acronym = doc/perm\.texi

# sc_prohibit_strcmp is broken because it gives false positives for
# cases where neither argument is a string literal.
local-checks-to-skip += sc_prohibit_strcmp


# NEWS hash.  We use this to detect unintended edits to bits of the NEWS file
# other than the most recent section.   If you do need to retrospectively update
# a historic section, run "make update-NEWS-hash", which will then edit this file.
old_NEWS_hash := d41d8cd98f00b204e9800998ecf8427e
