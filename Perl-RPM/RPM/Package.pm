###############################################################################
#
# This file copyright (c) 2000 by Randy J. Ray, all rights reserved
#
# Copying and distribution are permitted under the terms of the Artistic
# License as distributed with Perl versions 5.002 and later.
#
###############################################################################
#
#   $Id: Package.pm,v 1.10 2001/04/27 09:05:21 rjray Exp $
#
#   Description:    Perl-level glue and such for the RPM::Package class, the
#                   methods and accessors to package operations.
#
#   Functions:      AUTOLOAD
#
#   Libraries:      RPM
#
#   Global Consts:  $VERSION
#
#   Environment:    None.
#
###############################################################################

package RPM::Package;

use 5.005;
use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $AUTOLOAD);
use subs qw(AUTOLOAD);

require Exporter;

use RPM;

$VERSION = do { my @r=(q$Revision: 1.10 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

@ISA = qw(Exporter);
@EXPORT = ();
@EXPORT_OK = qw(RPM_PACKAGE_MASK RPM_PACKAGE_NOREAD RPM_PACKAGE_READONLY);
%EXPORT_TAGS = (all => \@EXPORT_OK);

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

1;

__END__
