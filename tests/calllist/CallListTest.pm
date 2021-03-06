package CallListTest;

use warnings;
use strict;

BEGIN {
	print "CallListTest.pm: BEGIN1\n";
}

CHECK {
	print "CallListTest.pm: CHECK1\n";
}

INIT {
	print "CallListTest.pm: INIT1\n";
}


libpperl::prologue(sub {
	print 'CallListTest.pm: prologue(' . join(', ', @ARGV) . ")\n";
});

libpperl::epilogue(sub {
	print 'CallListTest.pm: epilogue(' . join(', ', @ARGV) . "): $@\n";
});

print 'CallListTest.pm: body(' . join(', ', @ARGV) . ")\n";


END {
	print "CallListTest.pm: END: $?\n";
}

BEGIN {
	print "CallListTest.pm: BEGIN2\n";
}

CHECK {
	print "CallListTest.pm: CHECK2\n";
}

INIT {
	print "CallListTest.pm: INIT2\n";
}

1;
