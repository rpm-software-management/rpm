###############################################################################
#
# This file copyright (c) 2000 by Randy J. Ray, all rights reserved
#
# Copying and distribution are permitted under the terms of the Artistic
# License as distributed with Perl versions 5.002 and later.
#
###############################################################################
#
#   $Id: Package.pm,v 1.3 2000/10/05 04:48:59 rjray Exp $
#
#   Description:    Perl-level glue and such for the RPM::Package class, the
#                   methods and accessors to package operations.
#
#   Functions:      
#
#   Libraries:      RPM
#                   RPM::Header
#
#   Global Consts:  
#
#   Environment:    
#
###############################################################################

package RPM::Package;

use 5.005;
use strict;
use vars qw($VERSION $revision);
use subs qw();

use RPM;
use RPM::Header;

$VERSION = '0.29';
$revision = do { my @r=(q$Revision: 1.3 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

1;

__END__
