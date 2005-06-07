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
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

#
# Checks for programs.
#
AC_PROG_CC
AC_PATH_PROGS(PERL, [perl5.8 perl5 perl])

# Enable pretty strict warnings if we are using GCC.
# This is intended to catch bugs during development; do not remove these
# to silence warnings.
#
# At some point, I'de like to enable the following additional warnings
# but right now the perl headers have lots of violations which we
# cannot do anything about:
#		 -Wshadow
#		 -Wredundant-decls
#		 -Wnested-externs
#
if test "X$GCC" = "Xyes"; then
	for i in ${WERROR} -W -Wall	\
		 -Wstrict-prototypes	\
		 -Wmissing-prototypes	\
		 -Wpointer-arith	\
		 -Wreturn-type		\
		 -Wcast-qual		\
		 -Wwrite-strings	\
		 -Wswitch		\
		 -Wno-format-y2k	\
		 -Wcast-align		\
		 -Wchar-subscripts	\
		 -Winline
	do
		AM_CFLAGS="${AM_CFLAGS} $i"
	done
fi
AC_SUBST(AM_CFLAGS)

#
# Checks for libraries.
#
AC_CHECK_LIB(sbuf, sbuf_new, [system_libsbuf=true], [system_libsbuf=false])
AM_CONDITIONAL(SYSTEM_LIBSBUF, test x$system_libsbuf = xtrue)

#
# Checks for header files.
#
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h syslog.h unistd.h])

#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_RESTRICT
AC_C_INLINE
AC_TYPE_SIZE_T

#
# Checks for library functions.
#


# Note: the AC_FUNC_MALLOC, AC_FUNC_REALLOC, and AC_FUNC_MMAP macros are
#	bogus for our purposes:
#
#   * Both the AC_FUNC_MALLOC and AC_FUNC_REALLOC macros actually test for
#     GNU libc compatibility which we do not require as we don't assume glibc.
#
#   * The AC_FUNC_MMAP macro checks that mmap() works correctly for mapping to
#     specific addresses, that private mappings are truly private, and that
#     the mapped file is coherent with the on-disk image.  We only use mmap()
#     as a cheap replacement for malloc()/read() so the AC_FUNC_MMAP macro is
#     overkill.

AC_CHECK_FUNCS([dup2 strchr strdup strerror strrchr])

AC_CONFIG_FILES([
	Makefile
	libpperl/Makefile
	tests/Makefile
	pkgconfig/Makefile
	pkgconfig/libpperl.pc
])
AC_OUTPUT
