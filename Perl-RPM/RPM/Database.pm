###############################################################################
#
#   (c) Copyright @ 2000, Randy J. Ray <rjray@blackperl.com>
#               All Rights Reserved
#
###############################################################################
#
#   $Id: Database.pm,v 1.13 2000/11/10 09:55:51 rjray Exp $
#
#   Description:    The RPM::Database class provides access to the RPM database
#                   as a tied hash, whose keys are taken as the names of
#                   packages installed and whose values are RPM::Header
#                   objects.
#
#   Functions:      new
#                   import
#
#   Libraries:      RPM
#                   RPM::Header
#
#   Global Consts:  None.
#
#   Environment:    Just stuff from the RPM config.
#
###############################################################################

package RPM::Database;

require 5.005;

use strict;
use vars qw($VERSION $revision %RPM $RPM);
use subs qw(new import);

require RPM;
require RPM::Header;

$VERSION = '0.292';
$revision = do { my @r=(q$Revision: 1.13 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

1;

sub new
{
    my $class = shift;
    my %hash = ();

    tie %hash, $class;
}

###############################################################################
#
#   Sub Name:       import
#
#   Description:    This lies in wait to see if someone actually tries to
#                   import from this package. If they do, check that it is
#                   allowed and do it if it is.
#
#                   At present, only "%RPM" or "$RPM" is allowed. If that is
#                   the requested export, then the hash is tied to the RPM
#                   database (with no extra arguments) then exported to the
#                   caller's namespace.
#
#   Arguments:      NAME      IN/OUT  TYPE      DESCRIPTION
#                   $class    in      scalar    Name of this package
#                   @args     in      list      Requested exports
#
#   Globals:        None.
#
#   Environment:    None.
#
#   Returns:        void context
#
###############################################################################
sub import
{
    my ($class, @args) = @_;

    my $callpkg = caller(0);

    no strict 'refs';

    for my $arg (@args)
    {
        if ($arg eq '%RPM' and ! (tied %RPM))
        {
            tie %RPM, "RPM::Database";
            *{"${callpkg}::RPM"} = \%{"${class}::RPM"};
        }
        elsif ($arg eq '$RPM' and ! $RPM)
        {
            tie %RPM, "RPM::Database" unless (tied %RPM);
            $RPM = (tied %RPM);
            *{"${callpkg}::RPM"} = \${"${class}::RPM"};
        }
        else
        {
            warn "$class: Unknown export requested: $arg, ";
            return;
        }
    }
}

__END__

=head1 NAME

RPM::Database - Access to the RPM database of installed packages

=head1 SYNOPSIS

    use RPM::Database;

    tie %RPM, "RPM::Database" or die "$RPM::err";

    for (sort keys %RPM)
    {
        ...
    }

=head1 DESCRIPTION

The B<RPM::Database> package provides access to the database of installed
packages on a system. The database may be accessed as either a tied hash
or as a blessed reference to a hash. The keys of the hash represent
packages on the system. The order in which they are returned by any of
C<keys>, C<each> or C<values>, is determined by the internal database
ordering. Unlike the keys in B<RPM::Header> (see L<RPM::Header>), the
keys here are in fact case-sensitive.

The return value corresponding to each key is a reference to a
B<RPM::Header> object. The header object is marked read-only, as the
RPM database is not directly modifiable via this interface.

There are also a number of class methods implemented, which are described in
the next section.

=head1 USAGE

=head2 Creating an Object

An object may be created one of two ways:

    tie %D, "RPM::Database";

    $dataref = new RPM::Database;

The latter approach offers more direct access to the class methods, while
also permitting the usual tied-hash operations such as fetching:

    $dataref->{package}    # Such as "rpm" or "perl"

=head2 Class Methods

The following methods are available to objects of this class, in addition to
the tied-hash suite of operations. If the object is a hash instead of a
hash reference, it can be used to call these methods via:

    (tied %hash)->method_name(...)

=over

=item init

This causes a complete initialization of the RPM database. It must be run
with sufficient permissions to create/update the relevant files. It must
also be called as a static method, to avoid having any file descriptors
open on the database at the time.

=item rebuild

This rebuilds the database (same as "rpm --rebuilddb"). As with B<init>
above, this requires adequate permissions and must be invoked as a static
method.

=item find_by_file(file)

Returns a list of B<RPM::Header> objects that correspond to the package(s)
claiming ownership of the file "file".

=item find_by_group(group)

Returns of a list of headers for all packages flagged as being in the
group specified.

=item find_by_provides(provides)

Search as above, but based on which packages provide the file/object
specified as "provides".

=item find_by_required_by(requires)

Return a list of headers for the packages that directly depend on the
specified package for installation and operation.

=item find_by_conflicts(conflicts)

List those packages that have conflicts based on the value of "conflicts".

=item find_by_package(package)

This performs the search by a specific package name. This is the API call
used by the FETCH tied-hash method, but this differs in that if there is
in fact more than one matching record, all are returned.

=back

=head2 Importable Defaults

Given that there may be only one concurrent process with the rpm database
open, and given that such would lead to a lot of program code starting with
the same sequence of use/tie or use/new, the following identifiers may be
imported from the package:

=over

=item %RPM

A hash pre-tied to the RPM::Database package (and thus the rpm database).

=item $RPM

A RPM::Database object, referencing a hash tied to the rpm database.

=back

=head1 DIAGNOSTICS

Direct binding to the internal error-management of B<rpm> is still under
development. At present, most operations generate their diagnostics to
STDERR.

=head1 CAVEATS

This is currently regarded as alpha-quality software. The interface is
subject to change in future releases.

=head1 SEE ALSO

L<RPM>, L<RPM::Header>, L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>

=cut
