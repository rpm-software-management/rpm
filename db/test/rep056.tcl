# See the file LICENSE for redistribution information.
#
# Copyright (c) 2005-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep056.tcl,v 1.6 2006/08/24 14:46:38 bostic Exp $
#
# TEST  rep056
# TEST	Replication test with in-memory named databases.
# TEST
# TEST	Rep056 is just a driver to run rep001 with in-memory
# TEST	named databases.

proc rep056 { method args } {
	source ./include.tcl

	# Valid for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	eval { rep001 $method 1000 "056" } $args
}
