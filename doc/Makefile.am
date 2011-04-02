AM_CFLAGS = $(WARN_CFLAGS)

info_TEXINFOS = find.texi find-maint.texi
find_TEXINFOS = perm.texi parse-datetime.texi regexprops.texi fdl.texi
find_maint_TEXINFOS = fdl.texi
MOSTLYCLEANFILES = find.cps
CLEANFILES = find.txt find_mono.html findutils.texi_html_node.tar.gz

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
