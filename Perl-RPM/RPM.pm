package RPM;

use 5.005;
use strict;
use subs qw(bootstrap_Constants bootstrap_Header bootstrap_Database);
use vars qw($VERSION @ISA);

require DynaLoader;

@ISA = qw(DynaLoader);
$VERSION = '0.1';

bootstrap RPM $VERSION;

# These are stubs into the sub-module bootstraps, hacked into RPM.xs
bootstrap_Constants($VERSION);
bootstrap_Header($VERSION);
bootstrap_Database($VERSION);

1;

__END__

=head1 NAME

RPM - Perl interface to the API for the RPM Package Manager

=head1 SYNOPSIS

    use RPM;

    $pkg = new RPM "file.arch.rpm";

=head1 DESCRIPTION

At present, the package-manipulation functionality is not yet implemented.
The B<RPM::Database> and B<RPM::Header> packages do provide access to the
information contained within the database of installed packages, and
individual package headers, respectively.

=head1 DIAGNOSTICS

Direct binding to the internal error-management of B<rpm> is still under
development. At present, most operations generate their diagnostics to
STDERR.

=head1 CAVEATS

This is currently regarded as alpha-quality software. The interface is
subject to change in future releases.

=head1 SEE ALSO

L<RPM::Database>, L<RPM::Header>, L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>
