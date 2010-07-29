#!/bin/sh

rm -f sources.am
echo libgolly_a_SOURCES = `ls *.cpp \
	| grep -Ev '^wx|^bgolly|^RuleTableToTree'` >>sources.am
echo golly_SOURCES = wx*.cpp >>sources.am
echo nobase_dist_pkgdata_DATA = `find Help Patterns Rules Scripts -type f \
	| grep -Ev '/CVS|/[.]|[.]pyc$$'` >> sources.am
echo dist_doc_DATA = LICENSE README >> sources.am
echo EXTRA_DIST = BUILD TODO autogen.sh makefile-{gtk,mac,win} Info.plist.in \
	*.{h,rc,ico,icns,xpm,mk} bitmaps/*{.bmp,.xpm} >>sources.am

aclocal -I m4
automake --add-missing --copy
autoconf
