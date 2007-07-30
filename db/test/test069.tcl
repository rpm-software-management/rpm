# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999,2007 Oracle.  All rights reserved.
#
# $Id: test069.tcl,v 12.5 2007/05/17 15:15:56 bostic Exp $
#
# TEST	test069
# TEST	Test of DB_CURRENT partial puts without duplicates-- test067 w/
# TEST	small ndups to ensure that partial puts to DB_CURRENT work
# TEST	correctly in the absence of duplicate pages.
proc test069 { method {ndups 50} {tnum "069"} args } {
	eval test067 $method $ndups $tnum $args
}
