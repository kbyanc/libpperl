#!/usr/bin/perl

use warnings;
use strict;
use CallListTest;

BEGIN {
	print "calllist-test.pl: BEGIN\n";

	libpperl::prologue(sub {
		print 'calllist-test.pl: prologue(' . join(', ', @ARGV) . ")\n";
	});

	libpperl::epilogue(sub {
		print 'calllist-test.pl: epilogue(' . join(', ', @ARGV) . "): $@\n";
	});
}

CHECK {
	print "calllist-test.pl: CHECK\n";
}

INIT {
	print "calllist-test.pl: INIT\n";
}

print 'calllist-test.pl: body(' . join(', ', @ARGV) . ")\n";

die "test die on 3" if $ARGV[0] == 3;

END {
	print "calllist-test.pl: END: $?\n";
}
