#!/usr/bin/perl

use RPM::Header;
use RPM::Database;

chomp($rpmstr = qx{rpm -q rpm});

print "1..17\n";

tie %DB, "RPM::Database" or die "$RPM::err";

# Are we getting RPM::Header objects from the database?
$hdr = $DB{rpm};
print "not " unless (ref($hdr) and $hdr->isa('RPM::Header'));
print "ok 1\n";

# Does this one match what rpm thinks?
print "not "
    unless ($rpmstr eq join('-',
                            map { $hdr->{$_} } qw(name version release)));
print "ok 2\n";

# This is a much more involved test sequence
@rpmlines = `rpm -ql rpm`;
chomp(@rpmlines);

# Headers store files as a list of basenames, dirnames, and pointers to a dir
# for each file.
$files   = $hdr->{basenames};
$dirs    = $hdr->{dirnames};
$indices = $hdr->{dirindexes};

print "not " unless (@$files == @$indices);
print "ok 3\n";

print "not " unless (@$files == @rpmlines);
print "ok 4\n";

for $idx (0 .. $#rpmlines)
{
    if ($rpmlines[$idx] ne
        sprintf("%s%s", $dirs->[$indices->[$idx]], $files->[$idx]))
    {
        print "not ";
        last;
    }
}
print "ok 5\n";

# Starting with 0.27, we have a method to do that for you
$rpmlines = $hdr->filenames;
print "not " unless ($rpmlines and (@$rpmlines == @rpmlines));
print "ok 6\n";

for $idx (0 .. $#rpmlines)
{
    if ($rpmlines[$idx] ne $rpmlines->[$idx])
    {
        print "not ";
        last;
    }
}
print "ok 7\n";

# Can't really test RPM::Header->size(), except to see that it works.
print "not " if ($hdr->size <= 0);
print "ok 8\n";

# Check tagtype()
use RPM::Constants ':rpmtype';

print "not " unless ($hdr->tagtype(q{size}) == RPM_INT32_TYPE);
print "ok 9\n";

print "not " unless ($hdr->tagtype(q{dirnames}) == RPM_STRING_ARRAY_TYPE);
print "ok 10\n";

# Test the NVR method
print "not " unless ($rpmstr eq join('-', $hdr->NVR));
print "ok 11\n";

# Some tests on empty RPM::Header objects
$hdr = new RPM::Header;

print "not " unless (defined $hdr and (ref($hdr) eq 'RPM::Header'));
print "ok 12\n";

print "not " if (scalar($hdr->NVR));
print "ok 13\n";

# And now the scalar_tag predicate:
print "not " unless (RPM::Header->scalar_tag(q{size}));
print "ok 14\n";

use RPM::Constants ':rpmtag';
print "not " if (RPM::Header->scalar_tag(RPMTAG_DIRNAMES));
print "ok 15\n";

use RPM::Constants ':rpmerr';
print "not " unless ((! RPM::Header->scalar_tag(q{not_a_tag})) and
                     ($RPM::err == RPMERR_BADARG));
print "ok 16\n";

# Check all the keys to see that the scalar_tag flag matches the return value
$hdr = $DB{rpm};
while (($k, $v) = each %$hdr)
{
    unless ((ref($v) and (! $hdr->scalar_tag($k))) or
            ((! ref($v)) and $hdr->scalar_tag($k)))
    {
        print "not ";
        last;
    }
}
print "ok 17\n";

exit 0;
