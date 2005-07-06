#
# Makefile for building libpperl.
#
# $NTTMCL$
#

lib_LTLIBRARIES=	libpperl.la

include_HEADERS=	pperl.h

libpperl_la_SOURCES=	perlxsi.c \
			pperl.c \
			pperl_args.c \
			pperl_calllist.c \
			pperl_env.c \
			pperl_file.c \
			pperl_io.c \
			pperl_log.c \
			pperl_malloc.c \
			sbuf.c

libpperl_la_CPPFLAGS=	`perl -MExtUtils::Embed -e ccopts`
libpperl_la_LDFLAGS=	`perl -MExtUtils::Embed -e ldopts`

BUILT_SOURCES=		perlxsi.c
DISTCLEANFILES=		$(BUILT_SOURCES)

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c
