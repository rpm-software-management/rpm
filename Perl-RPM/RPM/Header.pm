###############################################################################
#
#   (c) Copyright @ 2000, Randy J. Ray <rjray@blackperl.com>
#               All Rights Reserved
#
###############################################################################
#
#   $Id: Header.pm,v 1.18 2001/03/07 19:17:25 rjray Exp $
#
#   Description:    The RPM::Header class provides access to the RPM Header
#                   structure as a tied hash, allowing direct access to the
#                   tags in a header as keys, and the content of said tags
#                   as the values.
#
#   Functions:      new
#                   AUTOLOAD
#                   filenames
#
#   Libraries:      None.
#
#   Global Consts:  None.
#
#   Environment:    Just stuff from the RPM config.
#
###############################################################################

package RPM::Header;

require 5.005;

use strict;
use vars qw($VERSION $revision @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $AUTOLOAD);
use subs qw(new AUTOLOAD filenames);

require Exporter;

use RPM;
use RPM::Error;
use RPM::Constants ':rpmerr';

$VERSION = '0.3';
$revision = do { my @r=(q$Revision: 1.18 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

@ISA = qw(Exporter);
@EXPORT = ();
@EXPORT_OK = qw(RPM_HEADER_MASK RPM_HEADER_READONLY);
%EXPORT_TAGS = (all => \@EXPORT_OK);

1;

sub AUTOLOAD
{
    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    die "& not defined" if $constname eq 'constant';
    my $val = constant($constname);
    if ($! != 0)
    {
        die "Your vendor has not defined RPM macro $constname";
    }
    no strict 'refs';
    *$AUTOLOAD = sub { $val };
    goto &$AUTOLOAD;
}

sub new
{
    my $class = shift;
    my %hash = ();

    tie %hash, $class, @_;
}

###############################################################################
#
#   Sub Name:       filenames
#
#   Description:    Glue together the contents of BASENAMES, DIRNAMES and
#                   DIRINDEXES to create a single list of fully-qualified
#                   filenames
#
#   Arguments:      NAME      IN/OUT  TYPE      DESCRIPTION
#                   $self     in      ref       Object of this class
#
#   Globals:        None.
#
#   Environment:    None.
#
#   Returns:        Success:    listref
#                   Failure:    undef
#
###############################################################################
sub filenames
{
    my $self = shift;

    # Each of these three fetches will have already triggered rpm_error, were
    # there any problems.
    my $base = $self->{BASENAMES}  || return undef;
    my $dirs = $self->{DIRNAMES}   || return undef;
    my $idxs = $self->{DIRINDEXES} || return undef;

    unless (@$base == @$idxs)
    {
        rpm_error(RPMERR_INTERNAL,
                  "Mis-matched elements lists for BASENAMES and DIRINDEXES");
        return undef;
    }

    # The value from DIRNAMES already has the trailing separator
    [ map { "$dirs->[$idxs->[$_]]$base->[$_]" } (0 .. $#$base) ];
}


__END__

=head1 NAME

RPM::Header - Access to RPM package headers

=head1 SYNOPSIS

    use RPM::Header;

    tie %hdr, "RPM::Header", "rpm-3.0.4-0.48.i386.rpm" or die "$RPM::err";

    for (sort keys %hdr)
    {
        ...
    }

=head1 DESCRIPTION

The B<RPM::Header> package permits access to the header of a package (external
file or currently installed) as either a tied hash or a blessed hash reference.
The tags that are present in the header are expressed as keys. Retrieving
them via C<keys> or C<each> returns the tags in the order in which they
appear in the header. Keys may be requested without regard for letter case,
but they are always returned as all upper-case.

The return value corresponding to each key is a list reference or scalar
(or C<undef> if the key is not valid), depending on the data-type of the
key. Each of the header tags are noted with one of C<$> or C<@> in the
B<RPM::Constants> documentation. The C<defined> keyword should be used
for testing success versus failure, as empty tags are possible. See the
C<scalar_tag> test, below.

This is a significant change from versions prior to 0.27, in which the
return value was always a list reference. This new approach brings
B<RPM::Header> more in line with other interfaces to B<rpm> header information.

B<RPM::Header> objects are also the native return value from keys retrieved
in the B<RPM::Database> class (see L<RPM::Database>). In these cases, the
header data is marked read-only, and attempts to alter any of the keys will
generate an error.

There are also a number of class methods implemented, which are described in
the next section.

=head1 USAGE

=head2 Creating an Object

An object may be created one of two ways:

    tie %h, "RPM::Header", "filename";

    $href = new RPM::Header "filename";

The latter approach offers more direct access to the class methods, while
also permitting the usual tied-hash operations such as fetching:

    $href->{tag}    # Such as "name" or "version"

=head2 Class Methods

The following methods are available to objects of this class, in addition to
the tied-hash suite of operations. If the object is a hash instead of a
hash reference, it can be used to call these methods via:

    (tied %hash)->method_name(...)

=over

=item filenames

The B<RPM> system attempts to save space by splitting up the file paths into
the leafs (stored by the tag C<BASENAMES>), the directories (stored under
C<DIRNAMES>) and indexes into the list of directories (stored under
C<DIRINDEXES>). As a convenience, this method re-assembles the list of
filenames and returns it as a list reference. If an error occurs, C<undef>
will be returned after C<rpm_error> has been called.

=item is_source

Returns true (1) or false (0), depending on whether the package the header
object is derived from is a source RPM.

=item size

Return the size of the header, in bytes, within the disk file containing the
associated package. The value is also returned for those headers within the
database.

=item scalar_tag(TAG)

Returns a true/false value (1 or 0) based on whether the value returned by
the specified tag is a scalar or an array reference. Useful in place of
using C<ref> to test the fetched values. B<TAG> may be either a string (name)
or a number (imported from the B<RPM::Constants> tag C<:rpmtag>). This
method may be called as a class (static) method.

=item tagtype(TAG)

Given a tag I<TAG>, return the type as a numerical value. The valid types
can be imported from the B<RPM::Constants> package via the import-tag
":rpmtype", and are:

=over

=item RPM_NULL_TYPE

Used internally by B<rpm>.

=item RPM_BIN_TYPE

The data returned is a single chunk of binary data. It has been converted to
a single "string" in Perl terms, but may contain nulls within it. The
B<length()> keyword should return the correct size for the chunk.

=item RPM_CHAR_TYPE

All items are single-character in nature. Note that since Perl does not
distinguish single characters versus strings in the way that C does, these
are stored as single-character strings. It is a tradeoff of efficiency over
memory.

=item RPM_INT8_TYPE

All items are integers no larger than 8 bits wide.

=item RPM_INT16_TYPE

All items are integers no larger than 16 bits wide.

=item RPM_INT32_TYPE

All items are integers no larger than 32 bits wide.

=item RPM_STRING_TYPE

=item RPM_I18NSTRING_TYPE

=item RPM_STRING_ARRAY_TYPE

These three are functionally similar from the Perl perspective. Currently,
B<RPM> does not export data in an I18N format, it will have been converted to
an ordinary string before being handed to the caller (in this case, before
the internal Perl/XS code receives it). The distinction between STRING and
STRING_ARRAY types is only relevant internally. All of these are sequences of
one or more text strings, returned in the same internal order as they are
stored within the header.

=back

=item NVR

The commonly-needed data triple of (B<name>, B<version>, B<release>) may be
accessed more directly by means of this method. It returns the three values
on the stack, saving the need for three separate fetches.

=item cmpver(OTHER)

Compare the version of the current header against that in the header
referenced by C<$other>. The argument should be an object reference, not
a tied-hash representation of a header. Returns -1, 0 or 1, based on the
established behavior of other comparison operators (C<cmp> and C<E<lt>=E<gt>>);
-1 indicates that the calling object is considered less, or older, than the
passed argument. A value of 1 indicates that the calling object is greater,
or newer, than the argument. A value of 0 indicates that they are equal.

=item source_name

If the B<RPM::Header> object is created directly from a file, FTP source
or HTTP source, then that source is kept for future reference and may be
retrieved using this accessor. This will be an empty string if the header
was retrieved from the RPM database, or was built in-memory from data.

=back

=head1 DIAGNOSTICS

Direct binding to the internal error-management of B<rpm> is still under
development. At present, most operations generate their diagnostics to
STDERR.

=head1 CAVEATS

This is currently regarded as alpha-quality software. The interface is
subject to change in future releases.

=head1 SEE ALSO

L<RPM>, L<RPM::Database>, L<RPM::Constants>, L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>

=cut
