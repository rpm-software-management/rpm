###############################################################################
#
#   (c) Copyright @ 2000, Red Hat Software, Inc.,
#               All Rights Reserved
#
###############################################################################
#
#   $Id: Header.pm,v 1.4 2000/06/14 09:28:38 rjray Exp $
#
#   Description:    The RPM::Header class provides access to the RPM Header
#                   structure as a tied hash, allowing direct access to the
#                   tags in a header as keys, and the content of said tags
#                   as the values.
#
#   Functions:      new
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
use vars qw($VERSION $revision);
use subs qw(new);

use RPM;
use RPM::Error;
use RPM::Constants ':rpmerr';

$VERSION = $RPM::VERSION;
$revision = do { my @r=(q$Revision: 1.4 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

1;

sub new
{
    my $class = shift;
    my %hash = ();

    tie %hash, $class, @_;
    return (tied %hash);
}

###############################################################################
#
#   Sub Name:       cmpver
#
#   Description:    Try to reasonably compare two headers by examining their
#                   version and release fields.
#
#   Arguments:      NAME      IN/OUT  TYPE      DESCRIPTION
#                   $self     in      ref       Object of this class
#                   $cmp_to   in      ref       Another object of this class
#
#   Globals:        None.
#
#   Environment:    None.
#
#   Returns:        Perl-style comparison operation
#
###############################################################################
sub cmpver
{
    my ($self, $cmp_to) = @_;

    # First off, arg 2 must be of or derived from this class
    unless ($cmp_to->isa('RPM::Header'))
    {
        rpm_error(RPMERR_BADARG,
                  "RPM::Header::cmpver: Argument 2 of wrong type");
        return 0;
    }
    my @self = $self->NVR;
    my @cmp_to = $cmp_to->NVR;
    # Secondly, comparison is meaningless if these are not the same package
    return 0 unless ($self[0] eq $cmp_to[0]);

    # Now it gets more complicated
    my @self_ver = split('.', $self[1]);
    my @self_rel = split('.', $self[2]);
    my @cmpto_ver = split('.', $cmp_to[1]);
    my @cmpto_rel = split('.', $cmp_to[2]);

    # If they do not have the same number of elements, pad the shorter one
    if (@self_ver < @cmpto_ver)
    {
        push(@self_ver, 0) for (1 .. (@cmpto_ver - @self_ver));
    }
    elsif (@self_ver > @cmpto_ver)
    {
        push(@cmpto_ver, 0) for (1 .. (@self_ver - @cmpto_ver));
    }
    for (0 .. $#self_ver)
    {
        return;
    }
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

The return value corresponding to each key is a list reference (or C<undef>
if the key is not valid). This is due to the fact that any of the tags may
have more than one piece of data associated, and the C<FETCH> interface to
hashes presumes scalar calling context and return value. Thus, rather than
require developers to frequently test the return value as a reference or
not, the value is simple always returned as a list ref, even if there is only
one element.

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

=item is_source

Returns true (1) or false (0), depending on whether the package the header
object is derived from is a source RPM.

=item size

Return the size of the header, in bytes, within the disk file containing the
associated package. The value is also returned for those headers within the
database.

=item tagtype(TAG)

Given a tag I<TAG>, return the type as a numerical value. The valid types
can be imported from the B<RPM::Constants> package via the import-tag
":rpmtype", and are:

=item NVR

The commonly-needed data triple of (B<name>, B<version>, B<release>) may be
accessed more directly by means of this method. It returns the three values
on the stack, with no need to dereference list references, as would be the
case when fetching the three tags via the usual means.

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

=back

=head1 DIAGNOSTICS

Direct binding to the internal error-management of B<rpm> is still under
development. At present, most operations generate their diagnostics to
STDERR.

=head1 CAVEATS

This is currently regarded as alpha-quality software. The interface is
subject to change in future releases.

=head1 SEE ALSO

L<RPM>, L<RPM::Database>, L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>

=cut
