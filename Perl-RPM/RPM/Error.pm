###############################################################################
#
# This file copyright (c) 2000 by Randy J. Ray, all rights reserved
#
# Copying and distribution are permitted under the terms of the Artistic
# License as distributed with Perl versions 5.002 and later.
#
###############################################################################
#
#   $Id: Error.pm,v 1.1 2000/05/27 03:53:56 rjray Exp $
#
#   Description:    Error-management support that cooperates with the primary
#                   Perl/C error glue.
#
#   Functions:      None (all XS)
#
#   Libraries:      RPM
#
#   Global Consts:  None.
#
#   Environment:    None.
#
###############################################################################

package RPM::Error;

use 5.005;
use strict;
use vars qw(@ISA $VERSION $revision @EXPORT @EXPORT_OK);

require Exporter;
require RPM;

@ISA = qw(Exporter);

$VERSION = $RPM::VERSION;
$revision = do { my @r=(q$Revision: 1.1 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

@EXPORT = qw(clear_errors set_error_callback rpm_error);
@EXPORT_OK = @EXPORT;

1;

__END__

=head1 NAME

RPM::Error - Functions to supplement the internal error management of RPM

=head1 SYNOPSIS

    use RPM::Error;
    use RPM::Constants ':rpmerr';

    set_error_callback(sub { ... });

    rpm_error(RPMERR_DBOPEN, "Error opening RPM DB: $!");

=head1 DESCRIPTION

The B<RPM::Error> package provides access to some functions that work with
(but do not replace) the special C<$RPM::err> variable. These routines allow
for reporting errors through the B<RPM> facility, clearing the error variable,
and registering a callback function to be invoked whenever a new error is
raised.

None of these routines are required for normal use of the special variable
C<$RPM::err> (see L<RPM>).

=head1 USAGE

The following routines are exported by B<RPM::Error>:

=over

=item rpm_error($code, $message)

Report an error through the internal facility used by B<rpm>. By using this
function, the special error variable is set up to have a dual-nature: When
evaluated in a numeric context, it returns the numerical code C<$code>. When
evaluated as a string, it will return C<$message>. The value of C<$code>
should be (but is not required to be) one of the values exported from the
B<RPM::Constants> package via the B<:rpmerr> tag. C<$message> may be any
string value.

=item clear_errors()

Clears both the numeric and string values of C<$RPM::err>.

=item $old_cb = set_error_callback($subr)

Set a (new) callback to be invoked whenever a new error is flagged. Returns
the old (existing) callback value if there was one.

The parameter to this call should be either a subroutine reference or a
closure. A subroutine name may be passed; if so, it should either be given
a package qualification or exist in the C<main::> namespace. The routine,
when invoked, will be passed the numeric code and the message string being
raised as the error. Note that the callback may be invoked by internal error
flagging in the core B<rpm> library, as well as by calls to B<rpm_error>
above.

=back

=head1 DIAGNOSTICS

If the value passed to B<set_error_callback> is not valid, the current
callback is set to a null value.

=head1 CAVEATS

The code value passed to B<rpm_error> is not checked against the list of
valid constants before assignment.

The B<set_error_callback> should return the current callback, which could then
be restored. This does not currently work correctly, and should not be used.

=head1 SEE ALSO

L<RPM>, L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>

=cut
