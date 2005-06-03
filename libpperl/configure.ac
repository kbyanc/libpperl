#
# Process this file with autoconf to produce a configure script.
#
# $NTTMCL$
#

AC_PREREQ(2.59)
AC_INIT([libpperl], [0.1], [kelly@nttmcl.com])

AC_CONFIG_SRCDIR([libpperl/pperl.c])
AC_CONFIG_HEADERS([libpperl/config.h])
AC_CONFIG_AUX_DIR([scripts])

AM_INIT_AUTOMAKE

# Allow shared libraries to be built using libtool.
# Note: building shared libraries is to remain disabled by default until the
#	1.0 release as the API is still subject to change.  I want to avoid
#	having to bump the library revision endlessly during development.
AC_DISABLE_SHARED
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

# Checks for programs.
AC_PROG_CC
AC_PATH_PROGS(PERL, [perl5.8 perl5 perl])

# Enable pretty strict warnings if we are using GCC.
# This is intended to catch bugs during development; do not remove these
# to silence warnings.
if test "X$GCC" = "Xyes"; then
	for i in ${WERROR} -W -Wall	\
		 -Wshadow		\
		 -Wstrict-prototypes	\
		 -Wmissing-prototypes	\
		 -Wpointer-arith	\
		 -Wreturn-type		\
		 -Wcast-qual		\
		 -Wwrite-strings	\
		 -Wswitch		\
		 -Wnon-const-format	\
		 -Wno-format-y2k	\
		 -Wcast-align		\
		 -Wunused-parameters	\
		 -Wchar-subscripts	\
		 -Winline		\
		 -Wnested-externs	\
		 -Wredundant-decls
	do
		AM_CFLAGS="${AM_CFLAGS} $i"
	done
fi
AC_SUBST(AM_CFLAGS)

# Checks for libraries.
AC_CHECK_LIB(sbuf, sbuf_new)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_RESTRICT
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 munmap strchr strdup strerror strrchr])

AC_CONFIG_FILES([
	Makefile
	pkgconfig/Makefile
	pkgconfig/libpperl.pc
	libpperl/Makefile
])
AC_OUTPUT
