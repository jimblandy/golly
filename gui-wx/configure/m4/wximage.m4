dnl ---------------------------------------------------------------------------
dnl Custom test to check for presence of specific image handlers in wxWindows.
dnl Assumes WX_CONFIG_CHECK has already passed.
dnl ---------------------------------------------------------------------------

AC_DEFUN([WX_CHECK_IMAGE_HANDLER], [
	AC_LANG_PUSH([C++])
	save_CXXFLAGS=$CXXFLAGS
	save_LIBS=$LIBS
	CXXFLAGS="$CXXFLAGS $WX_CXXFLAGS"
	LIBS="$LIBS $WX_LIBS"
	AC_MSG_CHECKING([for ]$1[ image handler])
	AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <wx/wx.h>
int main() { new wx]$1[Handler(); } ])],
		[AC_MSG_RESULT([yes]); AC_DEFINE([HAVE_WX_]$1[_HANDLER])],
		[AC_MSG_RESULT([no])] )
	CXXFLAGS=$save_CXXFLAGS
	LIBS=$save_LIBS
	AC_LANG_POP() ])
