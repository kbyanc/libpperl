#
# Makefile for building libpperl.
#
# $NTTMCL$
#

lib_LTLIBRARIES=	libpperl.la

libpperl_la_SOURCES=	perlxsi.c \
			pperl.c \
			pperl_args.c \
			pperl_calllist.c \
			pperl_env.c \
			pperl_file.c \
			pperl_io.c \
			pperl_log.c \
			pperl_malloc.c

libpperl_la_CPPFLAGS=	`perl -MExtUtils::Embed -e ccopts`
libpperl_la_LDFLAGS=	`perl -MExtUtils::Embed -e ldopts`

# Either link against the system's libsbuf or provide our own implementation.
# XXX There has got to be an easier way!
if SYSTEM_LIBSBUF
libpperl_la_LDFLAGS+=	-lsbuf
else
libpperl_la_SOURCES+=	${top_builddir}/libsbuf/subr_sbuf.c
libpperl_la_CPPFLAGS+=	-I${top_builddir}/libsbuf
endif

BUILT_SOURCES=		perlxsi.c
DISTCLEANFILES=		$(BUILT_SOURCES)

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c
