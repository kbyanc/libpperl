#
# Makefile for building libpperl.
#
# $NTTMCL$
#

lib_LTLIBRARIES=	libpperl.la

libpperl_la_SOURCES=	perlxsi.c \
			pperl.c \
			pperl_args.c \
			pperl_env.c \
			pperl_file.c \
			pperl_io.c \
			pperl_log.c \
			pperl_malloc.c

libpperl_la_CFLAGS=	`perl -MExtUtils::Embed -e ccopts`
libpperl_la_LDFLAGS=	`perl -MExtUtils::Embed -e ldopts`


BUILT_SOURCES=		perlxsi.c
DISTCLEANFILES=		$(BUILT_SOURCES)
#nodist_libpperl_la_SOURCES=	perlxsi.c

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c
