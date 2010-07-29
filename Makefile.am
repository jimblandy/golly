ACLOCAL_AMFLAGS = -I m4
AM_CXXFLAGS = -O2 -Wall -Wno-non-virtual-dtor -fno-strict-aliasing \
	-DGOLLYDIR="$(GOLLYDIR)"

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

bgolly_SOURCES = bgolly.cpp
bgolly_LDADD = libgolly.a

RuleTableToTree_SOURCES = RuleTableToTree.cpp
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

reallyclean: distclean
	rm -rf aclocal.m4 autom4te.cache config.guess config.log config.status \
		config.sub configure depcomp install-sh Makefile.in missing \
		sources.am $(srcdistdir) $(srcdistdir).tar.gz $(bindistdir) $(bindistdir).tar.gz
