AC_PREREQ(2.61)
AC_INIT([golly], [3.2])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall foreign no-define])
AC_CONFIG_SRCDIR([../../cmdline/bgolly.cpp])

# Custom configure arguments:
AC_ARG_WITH([zlib], [AS_HELP_STRING([--with-zlib],
	[support reading compressed pattern files with zlib])],
	, [with_zlib=yes])
AC_ARG_ENABLE([perl], [AS_HELP_STRING([--enable_perl],
	[support Perl scripts (deprecated)])],
	, [enable_perl=no])
AC_ARG_WITH([perl-shlib], [AS_HELP_STRING([--with-perl-shlib=ARG],
	[name of the Perl library when loaded dynamically (e.g. libperl.so.5.12)])],
	, [with_perl_shlib=check])
AC_ARG_WITH([python-shlib], [AS_HELP_STRING([--with-python-shlib=ARG],
	[name of the Python library when loaded dynamically (e.g. libpython2.6.so)])],
	, [with_python_shlib=check])
AC_ARG_VAR([GOLLYDIR], [golly data directory [default=DATADIR/golly]])
AC_ARG_VAR([PERL], [Perl 5 interpreter])
AC_ARG_VAR([PYTHON], [Python 2 interpreter])

# Check for build tools:
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_GREP
AC_PROG_EGREP
AC_PROG_SED
AC_PROG_RANLIB
AC_PROG_CXX
AC_CHECK_TOOL([OBJDUMP], [objdump])
AS_IF([test "x$OBJDUMP" = x], [AC_MSG_ERROR([missing objdump])])
AC_CHECK_TOOL([STRIP], [strip])

# Check for standard header files:
AC_CHECK_HEADERS([inttypes.h limits.h stdint.h stdlib.h string.h sys/time.h])

# Check for compiler characteristics:
AC_TYPE_SIZE_T

# Check for large file support (enabled by default)
AC_SYS_LARGEFILE

# Check for library functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset pow strchr strdup strrchr])

# Find wxWidgets
WX_CONFIG_OPTIONS
WX_STANDARD_OPTIONS([debug,unicode,shared,toolkit,wxshared])
WX_DEBUG=$DEBUG
WX_UNICODE=$UNICODE
WX_CONVERT_STANDARD_OPTIONS_TO_WXCONFIG_FLAGS
WX_CONFIG_CHECK([2.8.0], , [AC_MSG_ERROR([missing wxWidgets])],
	[std,gl], [$WXCONFIG_FLAGS])
WX_DETECT_STANDARD_OPTION_VALUES
AM_CONDITIONAL([RESCOMP], [test "x$WX_RESCOMP" != x])
WX_CHECK_IMAGE_HANDLER([BMP])
WX_CHECK_IMAGE_HANDLER([GIF])
WX_CHECK_IMAGE_HANDLER([PNG])
WX_CHECK_IMAGE_HANDLER([TIFF])

# Find Perl (if enabled)
AS_IF([test "x$enable_perl" = xyes], [
	AC_PATH_PROGS([PERL], [perl5 perl])
	AS_IF([test "x$PERL" = x], [AC_MSG_ERROR([missing Perl])])
	AC_SUBST([PERL_CPPFLAGS], [-DENABLE_PERL])
	AC_SUBST([PERL_INCLUDE], [`$PERL -MExtUtils::Embed -e ccopts`])
	AS_IF([test "x$with_perl_shlib" = xcheck],
		[CHECK_SHLIB_USED([shlib], [$PERL], [perl])],
		[shlib=$with_perl_shlib])
	AS_IF([test "x$shlib" = x],
		AC_MSG_ERROR([could not determine Perl shared library name]))
	AC_DEFINE_UNQUOTED([PERL_SHLIB], [$shlib])
])

# Find Python
AC_PATH_PROGS(PYTHON, [python2 python])
AS_IF([test "x$PYTHON" = x], [AC_MSG_ERROR([missing Python])])
AC_SUBST([PYTHON_INCLUDE], [-I"'`$PYTHON -c "import distutils.sysconfig; print(distutils.sysconfig.get_python_inc())"`'"])
AS_IF([test "x$with_python_shlib" = xcheck],
	[	[shlib=`$PYTHON -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('LDLIBRARY') or '')"`]
		AS_IF([test "x$shlib" = x], [CHECK_SHLIB_USED([shlib], [$PYTHON], [python])]) ],
	[	shlib=$with_python_shlib ] )
AS_IF([test "x$shlib" = x], AC_MSG_ERROR([could not determine Python shared library name]))
AC_DEFINE_UNQUOTED([PYTHON_SHLIB], [$shlib])

# Find zlib (unless explicitly disabled)
AS_IF([test "x$with_zlib" != xno],
	[	AC_CHECK_HEADER([zlib.h], , [AC_MSG_ERROR([missing zlib])])
		AC_SEARCH_LIBS([gzopen], [z], , [AC_MSG_ERROR([missing zlib])])
		AC_DEFINE(ZLIB) ] )

# Find library to link for dlopen (typically -ldl on Linux).
AC_SEARCH_LIBS([dlopen], [dl], , [AC_MSG_ERROR([missing dlopen])])

# Find library to link for OpenGL (typically -lGL).
AC_SEARCH_LIBS([glViewport], [GL], , [AC_MSG_ERROR([missing OpenGL])])

# Definitions used in the source:
AC_DEFINE_UNQUOTED([VERSION], [$PACKAGE_VERSION])
GOLLYDIR=${GOLLYDIR:-'${pkgdatadir}'}

# Determine which of three supported target operating systems we will compile
# for, and set a suffix for binary distributions accordingly:
case $target_os in
cygwin* | mingw* | pw32* | cegcc*) id=win ;;
darwin* | rhapsody*) id=mac ;;
linux*) id=gtk ;;
*) id= ;;
esac
case $target_cpu in
*64) bits=64 ;;
*) bits= ;;
esac
AC_SUBST([BINDISTSUFFIX], [${id:+-$id$bits}])
AC_SUBST([SRCDISTSUFFIX], [-src])
AM_CONDITIONAL([WINDOWS], [test x$id = xwin ])
AM_CONDITIONAL([MAC], [test x$id = xmac ])
AM_CONDITIONAL([LINUX], [test x$id = xgtk ])

# Generate output
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
WX_STANDARD_OPTIONS_SUMMARY_MSG
