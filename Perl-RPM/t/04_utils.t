#!/usr/bin/perl

# Test the basic util functions

use RPM;

print "1..2\n";

$arch = $os = '';

open(PIPE, "rpm --showrc |");
while (defined($line = <PIPE>))
{
    chomp $line;

    $line =~ /^build arch\s*:\s*(\S+)\s*$/ and $arch = $1;
    $line =~ /^build os\s*:\s*(\S+)\s*$/ and $os = $1;
}
close(PIPE);

print 'not ' unless ($arch eq GetArchName);
print "ok 1\n";

print 'not ' unless ($os eq GetOsName);
print "ok 2\n";

exit;
