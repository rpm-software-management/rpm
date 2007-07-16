# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: test081.tcl,v 12.3 2006/08/24 14:46:41 bostic Exp $
#
# TEST	test081
# TEST	Test off-page duplicates and overflow pages together with
# TEST	very large keys (key/data as file contents).
proc test081 { method {ndups 13} {tnum "081"} args} {
	source ./include.tcl

	eval {test017 $method 1 $ndups $tnum} $args
}
