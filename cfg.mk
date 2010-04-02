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

# Errors I think are too picky anyway.
skip_too_picky = sc_error_message_period sc_error_message_uppercase \
	sc_file_system

# Errors I have not investigated; diagnose and fix later.
skip_dunno = sc_immutable_NEWS sc_makefile_at_at_check \
	sc_prohibit_quote_without_use sc_prohibit_quotearg_without_use

# Understand, but fix later.
skip_defer = sc_program_name \
	sc_prohibit_magic_number_exit sc_prohibit_stat_st_blocks

# False positives I don't have a workaround for yet.
# sc_space_tab: several .xo test output files contain this sequence
#               for testing xargs's handling of white space.
false_positives = sc_obsolete_symbols sc_prohibit_cvs_keyword sc_the_the \
	sc_two_space_separator_in_usage \
	sc_space_tab

# Problems that have some false positives and some real ones; tease
# apart later.
mix_positives = sc_trailing_blank

# Problems partly fixed in other patches which aren't merged yet.
skip_blocked_patch = sc_useless_cpp_parens

# Problems we can't esaily fixed because they apply to files which we need
# to keep in sync, so can't easily make a local change to.
# sc_texinfo_acronym: perms.texi from coreutils uses @acronym{GNU}.
skip_blocked_notours = \
	sc_texinfo_acronym

# sc_prohibit_strcmp is broken because it gives false positives for cases
# where neither argument is a string literal.
skip_broken_checks = sc_prohibit_strcmp

local-checks-to-skip = \
	$(skip_too_picky) $(skip_dunno) $(false_positives) $(skip_defer) \
	$(mix_positives) $(skip_blocked_patch) $(skip_blocked_notours) $(skip_broken_checks)
