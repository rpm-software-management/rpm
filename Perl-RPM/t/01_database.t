#!/usr/bin/perl

use RPM::Database;

$SIG{__WARN__} = sub { $@ = shift; };
$SIG{__DIE__} = sub { $@ = shift; };

print "1..11\n";

tie %DB, "RPM::Database" or print "not ";
print "ok 1\n";

unless (tied %DB)
{
    die "$RPM::err";
    exit -1;
}

# This package must exist, obviously
$rpm = $DB{rpm};
print "not " unless (defined $rpm and ref $rpm);
print "ok 2\n";

# Verify that STORE, DELETE and CLEAR operations are blocked
# STORE
eval { $DB{foo_package} = 'baz'; print "not " if ($DB{foo_package} == 'baz') };
print "ok 3\n";

# DELETE
eval { delete $DB{foo_package} and print "not " };
print "ok 4\n";

# CLEAR
eval { %DB = () and print "not " };
print "ok 5\n";

# Test the untying
eval { untie %DB };
print "not " if ($@);
print "ok 6\n";

# That should cover the basic TIEHASH operands sufficiently.

# No way to test init() or rebuilddb() !!!

# All of the FindBy* suite behave basically the same way. For now, I only
# have these few tests...

# Test the non-tie approach
$rpm = new RPM::Database;
print "not " unless (defined $rpm and ref $rpm);
print "ok 7\n";

@matches = $rpm->find_by_file('/bin/rpm');
# There should be exactly one match:
print "not " unless (@matches == 1);
print "ok 8\n";

print "not " unless ($matches[0]->{name}->[0] eq 'rpm');
print "ok 9\n";

# There may be more than one package that depends on rpm
@matches = $rpm->find_by_required_by('rpm');
for (@matches) { $_ = $_->{name}->[0] }
# As long as we see this one (it has to be present to build this package)
print "not " unless (grep 'rpm-devel', @matches);
print "ok 10\n";

undef $rpm;
print "ok 11\n";

exit 0;
