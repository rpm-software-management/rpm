# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999,2007 Oracle.  All rights reserved.
#
# $Id: test071.tcl,v 12.5 2007/05/17 15:15:56 bostic Exp $
#
# TEST	test071
# TEST	Test of DB_CONSUME (One consumer, 10000 items.)
# TEST	This is DB Test 70, with one consumer, one producers, and 10000 items.
proc test071 { method {nconsumers 1} {nproducers 1} {nitems 10000} \
    {mode CONSUME} {start 0 } {txn -txn} {tnum "071"} args } {

	eval test070 $method \
	    $nconsumers $nproducers $nitems $mode $start $txn $tnum $args
}
