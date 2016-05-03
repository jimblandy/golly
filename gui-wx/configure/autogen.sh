#!/bin/bash -e

rm -f sources.am
echo nodist_liblua_a_SOURCES = ../../lua/*.{h,hpp,c} >>sources.am
echo libgolly_a_SOURCES = ../../gollybase/*.{h,cpp} >>sources.am
echo golly_SOURCES = ../../gui-wx/*.{h,cpp} >>sources.am
echo 'gollydatadir = $(pkgdatadir)/Patterns/Life/' >>sources.am
echo nobase_dist_gollydata_DATA = `find ../../{Help,Patterns,Rules,Scripts} -type f | sort` >>sources.am
echo EXTRA_DIST = \
	../../lua \
	../../gui-wx/{makefile-{gtk,mac,win},Info.plist.in,*.mk,*.rc,configure/autogen.sh,icons,bitmaps} \
	../../gui-common $(find ../../gui-{android,ios,web} -type f | sort) \
	../../docs >>sources.am

aclocal -I m4
automake --add-missing --copy
autoconf
