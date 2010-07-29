AC_PREREQ(2.64)
AC_INIT([golly], [2.2b4])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall -Werror foreign no-define])
AC_CONFIG_SRCDIR([bgolly.cpp])

# Custom configure arguments:
AC_ARG_WITH([zlib], [AS_HELP_STRING([--with-zlib],
	[support reading compressed pattern files with zlib])],
	, [with_zlib=yes])
AC_ARG_WITH([perl], [AS_HELP_STRING([--with-perl=PATH],
	[path to Perl interpreter to use [default=check]])],
	, [with_perl=check] )
AC_ARG_WITH([perl-shlib], [AS_HELP_STRING([--with-perl-shlib=ARG],
	[name of the Perl library when loaded dynamically (e.g. libperl.so.5.12)])],
	, [with_perl_shlib=check])
AC_ARG_WITH([python], [AS_HELP_STRING([--with-python=PATH],
	[path to Python interpreter to use [default=check]])],
	, [with_python=check] )
AC_ARG_WITH([python-shlib], [AS_HELP_STRING([--with-python-shlib=ARG],
	[name of the Python library when loaded dynamically (e.g. libpython2.6.so)])],
	, [with_python_shlib=check])
AC_ARG_VAR([GOLLYDIR], [golly data directory [default=DATADIR/golly]])
AC_ARG_VAR([PERL], [Perl 5 interpreter])
AC_ARG_VAR([PYTHON], [Python 2 interpreter])

# Check for build tools:
AC_PROG_GREP
AC_PROG_EGREP
AC_PROG_SED
AC_PROG_RANLIB
AC_PROG_CXX
AC_CHECK_TOOL([OBJDUMP], [objdump])
AS_VAR_IF([OBJDUMP], , [AC_MSG_ERROR([missing objdump])])

# Check for standard header files:
AC_CHECK_HEADERS([inttypes.h limits.h stdint.h stdlib.h string.h sys/time.h])

# Check for compiler characteristics:
AC_TYPE_SIZE_T

# Check for library functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset pow strchr strdup strrchr])

# Find wxWidgets
WX_CONFIG_OPTIONS
WX_CONFIG_CHECK([2.8.0], , [AC_MSG_ERROR([missing wxWidgets])]) 
AM_CONDITIONAL([RESCOMP], [test "x$WX_RESCOMP" != x])
WX_CHECK_IMAGE_HANDLER(BMP)
WX_CHECK_IMAGE_HANDLER(GIF)
WX_CHECK_IMAGE_HANDLER(PNG)
WX_CHECK_IMAGE_HANDLER(TIFF)

# Find Perl
AC_PATH_PROGS([PERL], [perl5 perl])
AS_VAR_IF([PERL], , [AC_MSG_ERROR([missing Perl])])
AC_SUBST([PERL_INCLUDE], [`$PERL -MExtUtils::Embed -e ccopts`])
AS_VAR_IF([with_perl_shlib], [check],
	[CHECK_SHLIB_USED([shlib], [$PERL], [perl])], [shlib=$with_perl_shlib] )
AS_VAR_IF([shlib], , AC_MSG_ERROR([could not determine Perl shared library name]))
AC_DEFINE_UNQUOTED([PERL_SHLIB], [$shlib])

# Find Python
AC_PATH_PROGS(PYTHON, [python2 python])
AS_VAR_IF([PYTHON], , [AC_MSG_ERROR([missing Python])])
AC_SUBST([PYTHON_INCLUDE], [-I"'`$PYTHON -c "import distutils.sysconfig; print(distutils.sysconfig.get_python_inc())"`'"])
AS_VAR_IF([with_python_shlib], [check],
	[	[shlib=`python -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('LDLIBRARY') or '')"`]
		AS_VAR_IF([shlib], , [CHECK_SHLIB_USED([shlib], [$PYTHON], [python])]) ],
	[	shlib=$with_python_shlib ] )
AS_VAR_IF([shlib], , AC_MSG_ERROR([could not determine Python shared library name]))
AC_DEFINE_UNQUOTED([PYTHON_SHLIB], [$shlib])

# Find zlib (unless explicitly disabled)
AS_VAR_IF([with_zlib], [no], ,
	[	AC_CHECK_HEADER([zlib.h], , [AC_MSG_ERROR([missing zlib])])
		AC_SEARCH_LIBS([gzopen], [z], , [AC_MSG_ERROR([missing zlib])])
		AC_DEFINE(ZLIB) ] )

# Definitions used in the source:
AC_DEFINE_UNQUOTED([VERSION], [$PACKAGE_VERSION])
GOLLYDIR=${GOLLYDIR:-'${pkgdatadir}'}

# Determine which of three supported target operating systems we will compile
# for, and set a suffix for binary distributions accordingly:
case $target_os in
cygwin* | mingw* | pw32* | cegcc*) id=win ;;
darwin* | rhapsody*) id=mac ;;
linux*) id=gtk ;;
*) tmp= ;;
esac
AC_SUBST([BINDISTSUFFIX], [${id:+-$id}])
AC_SUBST([SRCDISTSUFFIX], [-src])
AM_CONDITIONAL([WINDOWS], [test x$id = xwin ])
AM_CONDITIONAL([MAC], [test x$id = xmac ])
AM_CONDITIONAL([LINUX], [test x$id = xgtk ])

# Generate output
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
