#!/usr/bin/perl

use warnings;
use strict;
use CallListTest;

BEGIN {
	print "calllist-test.pl: BEGIN\n";
}

CHECK {
	print "calllist-test.pl: CHECK\n";
}

INIT {
	print "calllist-test.pl: INIT\n";
}

print 'calllist-test.pl: body(' . join(', ', @ARGV) . ")\n";

END {
	print "calllist-test.pl: END\n";
}
