ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I gollybase/
AM_CXXFLAGS = -DGOLLYDIR="$(GOLLYDIR)" -Wall -fno-strict-aliasing

bin_PROGRAMS = golly bgolly RuleTableToTree
noinst_LIBRARIES = libgolly.a

include sources.am

golly_CPPFLAGS = $(AM_CPPFLAGS) $(WX_CPPFLAGS) $(PERL_INCLUDE) $(PYTHON_INCLUDE)
golly_CXXFLAGS = $(AM_CXXFLAGS) $(WX_CXXFLAGS_ONLY)
golly_LDADD = $(WX_LIBS) libgolly.a

if WINDOWS
golly_LDADD += gollyres.o
gollyres.o: golly.rc; $(WX_RESCOMP) $< $@
endif

bgolly_SOURCES = cmdline/bgolly.cpp
bgolly_LDADD = libgolly.a

RuleTableToTree_SOURCES = cmdline/RuleTableToTree.cpp
RuleTableToTree_LDADD = libgolly.a

srcdistdir=$(PACKAGE)-$(VERSION)$(SRCDISTSUFFIX)
bindistdir=$(PACKAGE)-$(VERSION)$(BINDISTSUFFIX)

srcdist:
	make distdir=$(srcdistdir) dist

bindist:
	rm -rf "$(bindistdir)"
	make bindir=/ pkgdatadir=/ docdir=/ GOLLYDIR= clean all \
		DESTDIR="$(abs_builddir)/$(bindistdir)" install
	tardir=$(bindistdir) && $(am__tar) \
		| GZIP=$(GZIP_ENV) gzip -c >$(bindistdir).tar.gz
	rm -rf "$(bindistdir)"
