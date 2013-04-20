ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(top_srcdir)/../../gollybase/
AM_CXXFLAGS = -DGOLLYDIR="$(GOLLYDIR)" -Wall -fno-strict-aliasing
AM_LDFLAGS = -Wl,--as-needed

bin_PROGRAMS = golly bgolly
noinst_PROGRAMS = RuleTableToTree
noinst_LIBRARIES = libgolly.a
doc_DATA = ../../docs/License.html ../../docs/ReadMe.html
noinst_DATA = ../../docs/Build.html ../../docs/ToDo.html

include sources.am

golly_CPPFLAGS = $(AM_CPPFLAGS) $(WX_CPPFLAGS) $(PERL_INCLUDE) $(PYTHON_INCLUDE)
golly_CXXFLAGS = $(AM_CXXFLAGS) $(WX_CXXFLAGS_ONLY)
golly_LDADD = $(WX_LIBS) libgolly.a

if WINDOWS
golly_LDADD += gollyres.o
gollyres.o: ../golly.rc; $(WX_RESCOMP) $< $@
endif

bgolly_SOURCES = ../../cmdline/bgolly.cpp
bgolly_LDADD = libgolly.a

RuleTableToTree_SOURCES = ../../cmdline/RuleTableToTree.cpp
RuleTableToTree_LDADD = libgolly.a

srcdistdir=$(PACKAGE)-$(VERSION)$(SRCDISTSUFFIX)
bindistdir=$(PACKAGE)-$(VERSION)$(BINDISTSUFFIX)

srcdist:
	mkdir -p $(srcdistdir)/gui-wx/configure
	make distdir=$(srcdistdir)/gui-wx/configure distdir
	tardir=$(srcdistdir) && $(am__tar) \
		| GZIP=$(GZIP_ENV) gzip -c >$(srcdistdir).tar.gz
	rm -r $(srcdistdir)

bindist:
	make bindir=/ pkgdatadir=/ docdir=/ GOLLYDIR= \
		DESTDIR=$(builddir)/$(bindistdir) clean all install-strip
	tardir=$(bindistdir) && $(am__tar) \
		| GZIP=$(GZIP_ENV) gzip -c >$(bindistdir).tar.gz
	rm -r $(bindistdir)
