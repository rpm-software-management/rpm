#!/usr/bin/perl

use RPM::Database;

$SIG{__WARN__} = sub { $@ = shift; };
$SIG{__DIE__} = sub { $@ = shift; };

print "1..0\n";

exit;
