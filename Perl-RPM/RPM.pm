package RPM;

use 5.005;
use strict;
use subs qw(bootstrap_Constants bootstrap_Header bootstrap_Database);
use vars qw($VERSION $revision @ISA @EXPORT @EXPORT_OK);

require DynaLoader;
require Exporter;

@ISA = qw(Exporter DynaLoader);
$VERSION = '0.28';
$revision = do { my @r=(q$Revision: 1.8 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

@EXPORT = qw(rpm_osname rpm_archname);
@EXPORT_OK = @EXPORT;

bootstrap RPM $VERSION;

# These are stubs into the sub-module bootstraps, hacked into RPM.xs
bootstrap_Constants($VERSION);
bootstrap_Header($VERSION);
bootstrap_Database($VERSION);
bootstrap_Error($VERSION);

1;

__END__

=head1 NAME

RPM - Perl interface to the API for the RPM Package Manager

=head1 DESCRIPTION

The B<Perl-RPM> package is an extension for natively linking the
functionality of the B<RPM Package Manager> with the extension facility of
Perl. The aim is to offer all the functionality made available via the C
API in the form of Perl object classes.

At present, the package-manipulation functionality is not yet implemented.
The B<RPM::Database> and B<RPM::Header> packages do provide access to the
information contained within the database of installed packages, and
individual package headers, respectively. The B<RPM::Error> package is
available, which provides support routines for signaling and catching
errors. Additionally, there is the B<RPM::Constants> package which provides
a number of values from the B<rpm> library, referred to by the same name used
at the C level.

=head1 UTILITY FUNCTIONS

The following utility functions are exported by default from B<RPM>:

=over

=item rpm_osname

Returns the text name of the O/S, as derived from the B<rpm> configuration
files. This is the O/S token that B<rpm> will use to refer to the running
system.

=item rpm_archname

As above, but returns the architecture string instead. Again, this may not
directly match the running system, but rather is the value that B<rpm> is
using. B<rpm> will use the lowest-matching architecture whenever possible,
for maximum cross-platform compatibility.

=back

=head1 DIAGNOSTICS

When an error occurs in either the C-level B<rpm> library or internally
within these libraries, it is made available via a special dual-nature
variable B<$RPM::err>. When evaluated in a numeric context, it returns the
integer code value of the error. When taken in a string context, it returns
the text message associated with the error. This is intended to closely
mimic the behavior of the special Perl variable "C<$!>".

=head1 CAVEATS

This is currently regarded as alpha-quality software. The interface is
subject to change in future releases.

=head1 SEE ALSO

L<RPM::Constants>, L<RPM::Database>, L<RPM::Error>, L<RPM::Header>,
L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>

=cut
