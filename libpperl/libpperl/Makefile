#
# Makefile for building libpperl.
#
# $NTTMCL$
#

LIB=		pperl
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

PREFIX?=	/usr/local

INCSDIR=	${PREFIX}/include
LIBDIR=		${PREFIX}/lib

PERL_CCOPTS!=	perl -MExtUtils::Embed -e ccopts
PERL_LDOPTS!=	perl -MExtUtils::Embed -e ldopts

CFLAGS+=	${PERL_CCOPTS} -D_THREAD_SAFE
DEBUG_FLAGS+=	-g
LDADD+=		${PERL_LDOPTS}

WARNS?=		6
CWARNFLAGS+=	-Wno-format-y2k
CWARNFLAGS+=	-Wno-shadow

INCS=		pperl.h

SRCS=		perlxsi.c \
		pperl.c \
		pperl_args.c \
		pperl_env.c \
		pperl_file.c \
		pperl_io.c \
		pperl_log.c \
		pperl_malloc.c \

# Need to cleanup generated perlxsi.c too.
CLEANFILES+=	perlxsi.c

# All man pages are stored in a separate directory.
NOMAN=		noman

perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

.include <bsd.lib.mk>
