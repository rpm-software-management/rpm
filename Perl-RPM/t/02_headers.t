#!/usr/bin/perl

use RPM::Header;
use RPM::Database;

print "1..18\n";
$count = 1;

tie %DB, "RPM::Database" or die "$RPM::err";

#
# Find a package to use for package existence and data integrity testing.
#
for (qw(kernel rpm inetd bash))
{
    $test_pack = $_, last if (exists $DB{$_});
}
if ($test_pack)
{
    @test_files = `rpm -ql $test_pack`;
    chomp(@test_files);
    chomp($rpmstr = qx{rpm -q $test_pack});
}
else
{
    die "Not enough testable data in your RPM database, stopped";
}

# Are we getting RPM::Header objects from the database?
$hdr = $DB{$test_pack};
print "not " unless (ref($hdr) and $hdr->isa('RPM::Header'));
print "ok $count\n"; $count++;

# Does this one match what rpm thinks?
print "not "
    unless ($rpmstr eq join('-',
                            map { $hdr->{$_} } qw(name version release)));
print "ok $count\n"; $count++;

# Headers store files as a list of basenames, dirnames, and pointers to a dir
# for each file.
$files   = $hdr->{basenames};
$dirs    = $hdr->{dirnames};
$indices = $hdr->{dirindexes};

print "not " unless (@$files == @$indices);
print "ok $count\n"; $count++;

print "not " unless (@$files == @test_files);
print "ok $count\n"; $count++;

for $idx (0 .. $#test_files)
{
    if ($test_files[$idx] ne
        sprintf("%s%s", $dirs->[$indices->[$idx]], $files->[$idx]))
    {
        print "not ";
        last;
    }
}
print "ok $count\n"; $count++;

# Starting with 0.27, we have a method to do that for you
$rpmlines = $hdr->filenames;
print "not " unless ($rpmlines and (@$rpmlines == @test_files));
print "ok $count\n"; $count++;

for $idx (0 .. $#rpmlines)
{
    if ($test_files[$idx] ne $rpmlines->[$idx])
    {
        print "not ";
        last;
    }
}
print "ok $count\n"; $count++;

# Can't really test RPM::Header->size(), except to see that it works.
print "not " if ($hdr->size <= 0);
print "ok $count\n"; $count++;

# Check tagtype()
use RPM::Constants ':rpmtype';

print "not " unless ($hdr->tagtype(q{size}) == RPM_INT32_TYPE);
print "ok $count\n"; $count++;

print "not " unless ($hdr->tagtype(q{dirnames}) == RPM_STRING_ARRAY_TYPE);
print "ok $count\n"; $count++;

# Test the NVR method
print "not " unless ($rpmstr eq join('-', $hdr->NVR));
print "ok $count\n"; $count++;

# Some tests on empty RPM::Header objects
$hdr = new RPM::Header;

print "not " unless (defined $hdr and (ref($hdr) eq 'RPM::Header'));
print "ok $count\n"; $count++;

print "not " if (scalar($hdr->NVR));
print "ok $count\n"; $count++;

# And now the scalar_tag predicate:
print "not " unless (RPM::Header->scalar_tag(q{size}));
print "ok $count\n"; $count++;

use RPM::Constants ':rpmtag';
print "not " if (RPM::Header->scalar_tag(RPMTAG_DIRNAMES));
print "ok $count\n"; $count++;

use RPM::Constants ':rpmerr';
print "not " unless ((! RPM::Header->scalar_tag(q{not_a_tag})) and
                     ($RPM::err == RPMERR_BADARG));
print "ok $count\n"; $count++;

# Check all the keys to see that the scalar_tag flag matches the return value
$hdr = $DB{$test_pack};
while (($k, $v) = each %$hdr)
{
    unless ((ref($v) and (! $hdr->scalar_tag($k))) or
            ((! ref($v)) and $hdr->scalar_tag($k)))
    {
        print "not ";
        last;
    }
}
print "ok $count\n"; $count++;
untie %DB;

# Test an attempt to open a non-existant RPM file
$hdr = new RPM::Header "this_file_not_here.rpm";
print "not " if $hdr;
print "ok $count\n"; $count++;

exit 0;
