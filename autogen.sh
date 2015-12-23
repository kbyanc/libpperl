#!/bin/sh -e
#
# Script for generating autoconf scripts and automake Makefiles.
# The generated files are not checked into CVS.  They are generated
# as needed during development and during packaging (see also
# package/make-distfile).
#

if [ "X$1" = "Xclean" ]; then
	rm -f configure aclocal.m4 install-sh missing config.guess config.sub
	rm -f ltmain.sh
	rm -rf autom4te.cache scripts
	find . -name Makefile.in -delete
	find . -name config.h.in -delete
	exit 0
fi;

if [ ! -d scripts ]; then
	mkdir scripts
fi

libtoolize15 -c -f --automake

autoheader
aclocal
automake -a -c --foreign
autoconf

