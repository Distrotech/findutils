# Copyright (C) 1996,1999,2000,2001,2003-2015 Free Software Foundation, Inc.
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
AM_CFLAGS = $(WARN_CFLAGS)

info_TEXINFOS = find.texi find-maint.texi
find_TEXINFOS = perm.texi parse-datetime.texi regexprops.texi fdl.texi
BUILT_SOURCES = dblocation.texi
nodist_find_TEXINFOS = dblocation.texi
find_maint_TEXINFOS = fdl.texi
MOSTLYCLEANFILES = find.cps
CLEANFILES = find.txt find_mono.html findutils.texi_html_node.tar.gz dblocation.texi

MAKEINFOTXT = $(MAKEINFO) --plaintext

find.txt: find.texi $(srcdir)/version.texi $(find_TEXINFOS)

# find.txt is a file which we need to know how to build
# because it gets put on the www.gnu.org website.
# This rule is derived from the .texi.html rule.
.texi.txt:
	rm -rf $(@:.txt=.tmp)
	if $(MAKEINFOTXT) $(AM_MAKEINFOTXTFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 -o $(@:.txt=.tmp) $<; \
	then \
	  rm -rf $@; \
	  if test ! -d $(@:.txt=.tmp) && test -d $(@:.txt=); then \
	    mv $(@:.txt=) $@; else mv $(@:.txt=.tmp) $@; fi; \
	else \
	  if test ! -d $(@:.txt=.tmp) && test -d $(@:.txt=); then \
	    rm -rf $(@:.txt=); else rm -Rf $(@:.txt=.tmp) $@; fi; \
	  exit 1; \
	fi


# find_mono.html is a file which we need to know how to build
# because it gets put on the www.gnu.org website.
# This rule is derived from the generic .texi.html rule.
find_mono.html: find.texi
	rm -rf $(@:.html=.htp)
	if $(MAKEINFOHTML) --no-split $(AM_MAKEINFOHTMLFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 -o $(@:.html=.htp) $<; \
	then \
	  rm -rf $@; \
	  if test ! -d $(@:.html=.htp) && test -d $(@:.html=); then \
	    mv $(@:.html=) $@; else mv $(@:.html=.htp) $@; fi; \
	else \
	  if test ! -d $(@:.html=.htp) && test -d $(@:.html=); then \
	    rm -rf $(@:.html=); else rm -Rf $(@:.html=.htp) $@; fi; \
	  exit 1; \
	fi


# findutils.texi_html_node.tar.gz is a file which we need to know
# how to build because it gets put on the www.gnu.org website.
# This rule depends on GNU tar, but it's principally used
# by the maintainer, and we don't need to build the file
# for "make all" or "make install" (or even "make check").
findutils.texi_html_node.tar.gz: find.html
	tar zcf $@ $<


dblocation.texi: ../locate/dblocation.texi
	$(LN_S) ../locate/dblocation.texi $@
