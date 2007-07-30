# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000,2007 Oracle.  All rights reserved.
#
# $Id: test082.tcl,v 12.5 2007/05/17 15:15:56 bostic Exp $
#
# TEST	test082
# TEST	Test of DB_PREV_NODUP (uses test074).
proc test082 { method {dir -prevnodup} {nitems 100} {tnum "082"} args} {
	source ./include.tcl

	eval {test074 $method $dir $nitems $tnum} $args
}
