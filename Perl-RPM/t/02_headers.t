#!/usr/bin/perl

use RPM::Header;
use RPM::Database;

chomp($rpmstr = qx{rpm -q rpm});

print "1..8\n";

tie %DB, "RPM::Database" or die "$RPM::err";

# Are we getting RPM::Header objects from the database?
$hdr = $DB{rpm};
print "not " unless (ref($hdr) and $hdr->isa('RPM::Header'));
print "ok 1\n";

# Does this one match what rpm thinks?
print "not "
    unless ($rpmstr eq join('-',
                            map { $hdr->{$_}->[0] } qw(name version release)));
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

# Can't really test RPM::Header->size(), except to see that it works.
print "not " if ($hdr->size <= 0);
print "ok 6\n";

# Check tagtype()
use RPM::Constants ':rpmtype';

print "not " unless ($hdr->tagtype(q{size}) == RPM_INT32_TYPE);
print "ok 7\n";

print "not " unless ($hdr->tagtype(q{dirnames}) == RPM_STRING_ARRAY_TYPE);
print "ok 8\n";

exit 0;

