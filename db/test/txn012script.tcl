# See the file LICENSE for redistribution information.
#
# Copyright (c) 2005,2007 Oracle.  All rights reserved.
#
# $Id: txn012script.tcl,v 12.6 2007/05/17 15:15:56 bostic Exp $
#
# Script to check that txn names can be seen across processes.
# Names over 50 characters will be truncated.
#
# Usage: txn012script dir txnname longtxnname

source ./include.tcl
source $test_path/test.tcl

set usage "txn012script dir txnname longtxnname"

# Verify usage
if { $argc != 3 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set dir [ lindex $argv 0 ]
set txnname [ lindex $argv 1 ]
set longtxnname [ lindex $argv 2 ]

# Run db_stat to view txn names.
set stat [exec $util_path/db_stat -h $dir -t]
error_check_good txnname [is_substr $stat $txnname] 1
error_check_good longtxnname [is_substr $stat $longtxnname] 0
set truncname [string range $longtxnname 0 49]
error_check_good truncname [is_substr $stat $truncname] 1
