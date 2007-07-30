# See the file LICENSE for redistribution information.
#
# Copyright (c) 2005,2007 Oracle.  All rights reserved.
#
# $Id: fop008.tcl,v 12.5 2007/05/17 15:15:55 bostic Exp $
#
# TEST	fop008
# TEST	Test file system operations on named in-memory databases.
# TEST	Combine two ops in one transaction.
proc fop008 { method args } {
	eval {fop006 $method 1} $args
}



