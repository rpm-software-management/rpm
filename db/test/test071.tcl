# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: test071.tcl,v 11.13 2003/05/19 17:33:16 bostic Exp $
#
# TEST	test071
# TEST	Test of DB_CONSUME (One consumer, 10000 items.)
# TEST	This is DB Test 70, with one consumer, one producers, and 10000 items.
proc test071 { method {nconsumers 1} {nproducers 1} {nitems 10000} \
    {mode CONSUME} {start 0 } {txn -txn} {tnum "071"} args } {

	eval test070 $method \
	    $nconsumers $nproducers $nitems $mode $start $txn $tnum $args
}
