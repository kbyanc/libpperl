#
# $NTTMCL$
#

CFLAGS=		-I../../libpperl -Wall -W -O0 -g
LDFLAGS=	-L../../libpperl/.libs
LIBS=		-lpperl

all: calllist-test

calllist-test: calllist-test.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $>

clean:
	rm -f calllist-test calllist-test.o
	rm -f *.core

test: calllist-test
	./calllist-test | cmp -s -- - expected.output && echo "calllist-test: passed"
