# See the file LICENSE for redistribution information.
#
# Copyright (c) 2005-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: fop007.tcl,v 12.4 2006/08/24 14:46:35 bostic Exp $
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



