#
# $NTTMCL$
#

CFLAGS=		-I../../libpperl -Wall -W -O0 -g
LDFLAGS=	-L/usr/local/lib
LIBS=		-lpperl

args-test: args-test.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $>

clean:
	rm -f args-test args-test.o
	rm -f *.core
