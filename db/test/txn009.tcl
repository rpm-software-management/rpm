# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: txn009.tcl,v 11.5 2003/01/08 05:54:15 bostic Exp $
#

# TEST	txn009
# TEST	Test of wraparound txnids (txn003)
proc txn009 { } {
	source ./include.tcl
	global txn_curid
	global txn_maxid

	set orig_curid $txn_curid
	set orig_maxid $txn_maxid
	puts "\tTxn009.1: wraparound txnids"
	set txn_curid [expr $txn_maxid - 2]
	txn003 "009.1"
	puts "\tTxn009.2: closer wraparound txnids"
	set txn_curid [expr $txn_maxid - 3]
	set txn_maxid [expr $txn_maxid - 2]
	txn003 "009.2"

	puts "\tTxn009.3: test wraparound txnids"
	txn_idwrap_check $testdir
	set txn_curid $orig_curid
	set txn_maxid $orig_maxid
	return
}

