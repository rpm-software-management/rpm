#!/usr/bin/perl

use RPM::Database;

$SIG{__WARN__} = sub { $@ = shift; };
$SIG{__DIE__} = sub { $@ = shift; };

print "1..13\n";
$count = 1;

#
# Prior to starting up, we need to do some less-direct queries of the RPM
# database, so that we have baseline data to test against.
#
@all_packs = `rpm -q -a --queryformat "\%{NAME}\\n"`;
chomp(@all_packs);
$all_packs{$_}++ for (@all_packs);

#
# With a full list of packages now known, find one to use for package existence
# testing.
#
for (qw(rpm kernel bash file passwd))
{
    $test_pack = $_, last if (exists $all_packs{$_});
}

tie %DB, "RPM::Database" or print "not ";
print "ok $count\n"; $count++;

unless (tied %DB)
{
    die "$RPM::err";
    exit -1;
}

# Start with the test package
$rpm = $DB{$test_pack};
print "not " unless (defined $rpm and ref $rpm);
print "ok $count\n"; $count++;

# Verify that STORE, DELETE and CLEAR operations are blocked
# STORE
eval {
    $DB{foo_package} = 'baz';
    print "not " if (exists $DB{foo_package} and ($DB{foo_package} eq 'baz'));
};
print "ok $count\n"; $count++;

# DELETE
eval { delete $DB{foo_package} and print "not " };
print "ok $count\n"; $count++;

# CLEAR
eval { %DB = () and print "not " };
print "ok $count\n"; $count++;

# Test the untying
eval { untie %DB };
print "not " if ($@);
print "ok $count\n"; $count++;

# That should cover the basic TIEHASH operands sufficiently.

# No way to test init() or rebuilddb() !!!

# All of the FindBy* suite behave basically the same way. For now, I only
# have these few tests...

# Test the non-tie approach
$rpm = new RPM::Database;
print "not " unless (defined $rpm and ref $rpm);
print "ok $count\n"; $count++;

# Ensure that the same test package is visible
print "not " unless (exists $rpm->{$test_pack} and ref($rpm->{$test_pack}));
print "ok $count\n"; $count++;

@matches = $rpm->find_by_file('/bin/rpm');
# There should be exactly one match:
print "not " unless (@matches == 1);
print "ok $count\n"; $count++;

print "not " unless ($matches[0]->{name} eq 'rpm');
print "ok $count\n"; $count++;

# There may be more than one package that depends on rpm
@matches = $rpm->find_by_required_by('rpm');
for (@matches) { $_ = $_->{name} }
# As long as we see this one (it has to be present to build this package)
print "not " unless (grep($_ eq 'rpm-devel', @matches));
print "ok $count\n"; $count++;

# Try to fetch a bogus package
$hdr = $rpm->{i_hope_no_one_makes_a_package_by_this_name};
print "not " if $hdr;
print "ok $count\n"; $count++;

undef $rpm;
print "ok $count\n"; $count++;

exit 0;
