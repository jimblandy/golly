#!/bin/sh

DEST=gui-wx/configure/sources.am
set -e
rm -f "$DEST"
echo libgolly_a_SOURCES = gollybase/*.{h,cpp} >>"$DEST"
echo golly_SOURCES = gui-wx/*.{h,cpp} >>"$DEST"
echo nobase_dist_pkgdata_DATA = `find {Help,Patterns,Rules,Scripts} -type f | sort` >>"$DEST"
echo dist_doc_DATA = docs/*.html >>"$DEST"

echo EXTRA_DIST = gui-wx/{makefile-{gtk,mac,win},CMakeLists.txt,Info.plist.in,*.mk,*.rc} gui-wx/configure/autogen.sh gui-wx/icons/* gui-wx/bitmaps/* $(find gui-ios -type f | sort) >>"$DEST"

(cd gui-wx/configure/ && aclocal -I m4 && automake --add-missing --copy && autoconf)
