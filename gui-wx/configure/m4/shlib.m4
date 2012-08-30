dnl Hackish check to determine the exact name of a dynamic library required to
dnl run a specific executable.
dnl
dnl Usage: 
dnl     CHECK_SHLIB_USED(variable,exec-path,lib-name,value-if-not-found)
dnl Where:
dnl		`variable' is the name of the variable to be set to the full library name
dnl   `exec-path' is the full (absolute or relative) path to the executable
dnl   `lib-name' is the basename of the library (e.g. `perl' for `libperl.so')
dnl   `value-if-not-found' is the value set when the library is not found.  

AC_DEFUN([CHECK_SHLIB_USED], [
	AC_MSG_CHECKING([for $3 shared library used by $2])
	[$1=`$OBJDUMP -p "$2" \
		| $EGREP -i 'NEEDED +lib$3[0-9.]*[.]so|DLL Name: +$3[0-9.]*[.]dll' \
		| $SED 's/^.* //'`]
	AS_IF([test "x$]$1[" != x],
		[	AC_MSG_RESULT([$]$1) ],
		[	AC_MSG_RESULT([not found])
			$1=$4 ]) ])
