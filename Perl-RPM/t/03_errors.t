#!/usr/bin/perl

# Test the error-reporting and management functions

use RPM::Error;
use RPM::Constants ':rpmerr';

use vars qw($called $string $oldcb);

select(STDOUT); $| = 1;

print "1..11\n";

# tests 1-2: basic set
rpm_error(RPMERR_BADARG, "Bad argument passed");

print 'not ' unless ($RPM::err == RPMERR_BADARG);
print "ok 1\n";

print 'not ' unless ($RPM::err eq 'Bad argument passed');
print "ok 2\n";

# tests 3-4: clearing the values
clear_errors;

print 'not ' if ($RPM::err);
print "ok 3\n";

print 'not ' if (length($RPM::err));
print "ok 4\n";

# tests 5-8: callbacks
$called = 0;
set_error_callback(sub { $called = 1 });
rpm_error(RPMERR_BADSPEC, "Bad spec");

print 'not ' unless ($called);
print "ok 5\n";

sub cb1 { $called = shift; $string = shift; }
$called = 0; $string = '';
set_error_callback('cb1');
rpm_error(RPMERR_BADDEV, "baddev");

print 'not ' unless ($called == $RPM::err);
print "ok 6\n";

print 'not ' unless (length($string) == 6);
print "ok 7\n";

sub cb2 { $called = 1 }
set_error_callback(\&cb2);
rpm_error(RPMERR_BADMAGIC, "badmagic");

print 'not ' unless ($called == 1 and $RPM::err == RPMERR_BADMAGIC);
print "ok 8\n";

my $oldcb = set_error_callback(undef);
$called = 0;
rpm_error(RPMERR_BADDEV, "baddev");

print 'not ' if ($called);
print "ok 9\n";

print 'not ' unless (ref $oldcb eq 'CODE');
print "ok 10\n";

set_error_callback($oldcb);
$called = 0;
rpm_error(RPMERR_BADMAGIC, "badmagic");

print 'not ' unless ($called == 1 and $RPM::err == RPMERR_BADMAGIC);
print "ok 11\n";

exit 0;
