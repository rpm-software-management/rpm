# See the file LICENSE for redistribution information.
#
# Copyright (c) 2005,2007 Oracle.  All rights reserved.
#
# $Id: fop007.tcl,v 12.6 2007/05/17 15:15:55 bostic Exp $
#
# TEST	fop007
# TEST	Test file system operations on named in-memory databases.
# TEST	Combine two ops in one transaction.
proc fop007 { method args } {

	# Queue extents are not allowed with in-memory databases.
	if { [is_queueext $method] == 1 } {
		puts "Skipping fop007 for method $method."
		return
	}
	eval {fop001 $method 1} $args
}



