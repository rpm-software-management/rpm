###############################################################################
#
# This file copyright (c) 2000 by Randy J. Ray, all rights reserved
#
# Copying and distribution are permitted under the terms of the Artistic
# License as distributed with Perl versions 5.002 and later.
#
###############################################################################
#
#   $Id: Transaction.pm,v 1.8 2001/04/27 09:05:21 rjray Exp $
#
#   Description:    Perl-level glue and such for the RPM::Transaction class,
#                   the methods and accessors to transaction operations.
#
#   Functions:      
#
#   Libraries:      RPM
#                   RPM::Header
#                   RPM::Package
#
#   Global Consts:  
#
#   Environment:    
#
###############################################################################

package RPM::Package;

use 5.005;
use strict;
use vars qw($VERSION);
use subs qw();

use RPM;
use RPM::Header;
use RPM::Package;

$VERSION = do { my @r=(q$Revision: 1.8 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

1;

__END__
