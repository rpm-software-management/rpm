# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: test082.tcl,v 12.3 2006/08/24 14:46:41 bostic Exp $
#
# TEST	test082
# TEST	Test of DB_PREV_NODUP (uses test074).
proc test082 { method {dir -prevnodup} {nitems 100} {tnum "082"} args} {
	source ./include.tcl

	eval {test074 $method $dir $nitems $tnum} $args
}
