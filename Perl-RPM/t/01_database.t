#!/usr/bin/perl

use RPM::Database;

print "1..19\n";
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
for (qw(kernel rpm inetd bash))
{
    $test_pack = $_, last if (exists $all_packs{$_});
}
if ($test_pack)
{
    @test_requires = `rpm -q --requires $test_pack`;
    chomp(@test_requires);
    @test_requires = map { (split(/ /, $_))[0] } grep(! m|^/|, @test_requires);
    @test_requires = grep(! /^rpmlib\(/, @test_requires);
    @test_required_by = `rpm -q --whatrequires $test_pack`;
    chomp(@test_required_by);
    @test_required_by = map { @p = split('-', $_);
                              pop(@p); pop(@p);
                              join('-', @p); } (@test_required_by);
}
else
{
    die "Not enough testable data in your RPM database, stopped";
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

# Obviously, EXISTS should clear just fine
print "not " unless (exists $DB{$test_pack});
print "ok $count\n"; $count++;

# Run over FIRSTKEY and NEXTKEY by iterating against a copy of %all_packs
%tmp_packs = %all_packs;
for (keys %DB) { delete $tmp_packs{$_} }
print "not " if (keys %tmp_packs);
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

# Run over FIRSTKEY and NEXTKEY for the direct ref interface
%tmp_packs = %all_packs;
for (keys %$rpm) { delete $tmp_packs{$_} }
print "not " if (keys %tmp_packs);
print "ok $count\n"; $count++;

@matches = $rpm->find_by_file('/sbin/installkernel');
# There should be exactly one match:
print "not " unless (@matches == 1);
print "ok $count\n"; $count++;

print "not " unless ($matches[0]->{name} eq 'kernel');
print "ok $count\n"; $count++;

# There may be more than one package that depends on $test_pack
@matches = $rpm->find_what_requires($test_pack);
%test = ();
for (@matches) { $test{$_->{name}} = 1 }
for (@test_required_by) { delete $test{$_} }
print "not " if (keys %test);
print "ok $count\n"; $count++;

# Check now for finding those packages that $test_pack itself requires
for $testp (@test_requires)
{
    @matches = $rpm->find_what_requires($rpm->{$testp});
    print "not ", last unless (grep($_->{name} eq $test_pack, @matches));
}
print "ok $count\n"; $count++;

# Test the find-by-group
# First, check that the test pack is in the return list
@matches = $rpm->find_by_group($rpm->{$test_pack}->{group});
print "not " unless (grep($_->{name} eq $test_pack, @matches));
print "ok $count\n"; $count++;

# Check the list of matches against what RPM thinks
@by_group = `rpm -q --group '$rpm->{$test_pack}->{group}'`;
%test = ();
for (@by_group)
{
    @p = split '-';
    pop(@p); pop(@p);
    $_ = join('-', @p);
    $test{$_}++;
}
for (@matches)
{
    delete $test{$_->{name}};
}
print "not " if ((! scalar(@by_group)) || (keys %test));
print "ok $count\n"; $count++;

# Try to fetch a bogus package
$hdr = $rpm->{i_hope_no_one_makes_a_package_by_this_name};
print "not " if $hdr;
print "ok $count\n"; $count++;

undef $rpm;
print "ok $count\n"; $count++;

exit 0;
