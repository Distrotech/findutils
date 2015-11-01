# Copyright (C) 2001,2003-2015 Free Software Foundation, Inc.
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

# The 2001 copyright line above was added in 2015 based on James
# Youngman's best estimate of the original publication date of this
# file (it appeared in findutils release 4.1.7 but may have appeared
# in earlier releases which I don't have copies of).

AUTOMAKE_OPTIONS=dejagnu
LOCATE = ../locate
UPDATEDB = ../updatedb
LOCATEFLAGS =
UPDATEDBFLAGS =
DEJATOOL = locate

EXTRA_DIST_EXP = \
config/unix.exp \
locate.gnu/bigendian.exp \
locate.gnu/littleendian.exp \
locate.gnu/ignore_case1.exp \
locate.gnu/ignore_case2.exp \
locate.gnu/ignore_case3.exp \
locate.gnu/bigprefix1.exp \
locate.gnu/regex1.exp \
locate.gnu/exists1.exp \
locate.gnu/exists2.exp \
locate.gnu/exists3.exp \
locate.gnu/slocate.exp \
locate.gnu/notexists1.exp \
locate.gnu/notexists2.exp \
locate.gnu/notexists3.exp \
locate.gnu/old_prefix.exp \
locate.gnu/oldformat.exp \
locate.gnu/space1st.exp \
locate.gnu/sv-bug-14535.exp \
locate.gnu/exceedshort.exp

EXTRA_DIST_XI = \
locate.gnu/locateddb.old.powerpc.xi \
locate.gnu/locateddb.old.x86.xi

EXTRA_DIST_XO = \
locate.gnu/bigendian.xo \
locate.gnu/littleendian.xo \
locate.gnu/ignore_case1.xo \
locate.gnu/ignore_case2.xo \
locate.gnu/ignore_case3.xo \
locate.gnu/exists1.xo \
locate.gnu/exists2.xo \
locate.gnu/exists3.xo \
locate.gnu/slocate.xo \
locate.gnu/notexists1.xo \
locate.gnu/notexists2.xo \
locate.gnu/notexists3.xo \
locate.gnu/old_prefix.xo \
locate.gnu/oldformat.xo

EXTRA_DIST = $(EXTRA_DIST_EXP) $(EXTRA_DIST_XO) $(EXTRA_DIST_XI)

# DIST_SUBDIRS = config locate.gnu

CLEANFILES = *.log *.sum site.exp site.bak
