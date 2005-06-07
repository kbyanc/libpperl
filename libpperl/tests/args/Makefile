#
# $NTTMCL$
#

CFLAGS=		-I../../libpperl -Wall -W -O0 -g
LDFLAGS=	-L../../libpperl/.libs
LIBS=		-lpperl

all: args-test

args-test: args-test.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $>

clean:
	rm -f args-test args-test.o
	rm -f *.core

test: args-test
	./args-test | cmp -s -- - expected.output && echo "args-test: passed"
