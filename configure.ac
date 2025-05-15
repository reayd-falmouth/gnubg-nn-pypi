#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_REVISION($Revision: 1.8 $)
AC_INIT([gnubg-nn],[0.17],[bug-gnubg@gnu.org])
AC_CONFIG_MACRO_DIR([m4])
dnl AC_CONFIG_SRCDIR([py/py3mod.cpp])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

AC_DISABLE_SHARED
LT_INIT

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for header files.
AC_FUNC_ALLOCA
m4_warn([obsolete],
[The preprocessor macro `STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  ISO C90 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_CHECK_HEADERS([fcntl.h memory.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memmove memset pow sqrt strcasecmp strchr strdup strncasecmp])

AC_SEARCH_LIBS([dlopen], [dl dld], [], [
  AC_MSG_ERROR([unable to find the dlopen() function])
])

dnl
dnl SSE
dnl

AC_MSG_CHECKING([for sse cpu])
AC_ARG_ENABLE( sse, [  --enable-sse=TYPE  enable SSE for newer cpus(TYPE=yes,sse2,no,cputest)], ssecpu=$enableval, ssecpu="undef")
if test "x$ssecpu" = "xundef"; then
	case "$host" in
		i386-apple-darwin* )
		ssecpu="sse2"
		;;
		x86-* | i586-* | i686-* )
        	if test x"$GCC" = xyes; then
                	ssecpu="cputest"
		else
                	ssecpu="yes"
		fi
		;;
		ia64-* | x86_64-* | amd64-* )
		ssecpu="sse2"
		;;
		*)
		ssecpu="no"
		;;
	esac
fi
if test "x$ssecpu" != "xno"; then
        AC_DEFINE(USE_SSE_VECTORIZE,1,Define if you want to compile with SSE support)
        if test "x$ssecpu" != "xcputest"; then
                AC_DEFINE(DISABLE_SSE_TEST, 1, Define if you want to disable the SSE cpu test)
        fi
        if test "x$ssecpu" = "xsse2"; then
                AC_DEFINE(USE_SSE2, 1, Define if you want to compile with SSE2 support)
        fi
        if test x"$GCC" = xyes; then
		if test "x$SSE_CFLAGS" = x; then
			if test "x$ssecpu" = "xsse2"; then 
				SSE_CFLAGS="-msse -msse2"
			else
				SSE_CFLAGS="-msse"
			fi
		fi
        fi
fi

AC_MSG_RESULT([$host (sse=$ssecpu, SSE_CFLAGS="$SSE_CFLAGS")])

AC_ARG_VAR(SSE_CFLAGS, [CFLAGS needed for compiling in sse cpu support])
   
dnl

dnl AX_PYTHON_DEFAULT()
dnl AX_PYTHON_WITH()
dnl AX_PYTHON_VERSION_ENSURE([3.10])
dnl AX_PYTHON_CSPEC
dnl AX_PYTHON_LSPEC
dnl if test "x$ax_python_cspec" == "x"; then
dnl 	AC_MSG_ERROR("Python 3.10 or higher is required. Did you use --with-python=python3?");
dnl fi

AC_DEFINE(OS_BEAROFF_DB, 1, [osbdb])
AC_DEFINE(LOADED_BO, 1, [lbo])
AC_DEFINE(DOICC, 1, [doicc])
AC_DEFINE(DEBUG, 1, [debug])

AC_CONFIG_FILES([Makefile analyze/Makefile gnubg/Makefile gnubg/lib/Makefile sa/Makefile doc/Makefile scripts/Makefile m4/Makefile])
AC_OUTPUT
