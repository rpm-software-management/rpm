# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: fopscript.tcl,v 1.3 2003/09/04 23:41:11 bostic Exp $
#
# Fop006 script - test of fileops in multiple transactions
# Usage: fopscript
# op: file operation to perform
# end: how to end the transaction (abort or commit)
# result: expected result of the transaction
# args: name(s) of files to operate on

source ./include.tcl
source $test_path/test.tcl
source $test_path/testutils.tcl

set usage "fopscript op end result args"

# Verify usage
if { $argc != 4 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set op [ lindex $argv 0 ]
set end [ lindex $argv 1 ]
set result [ lindex $argv 2 ]
set args [ lindex $argv 3 ]

# Join the env
set dbenv [eval berkdb_env -home $testdir]
error_check_good envopen [is_valid_env $dbenv] TRUE

# Start transaction
puts "\tFopscript.a: begin 2nd transaction (will block)"
set txn2 [$dbenv txn]
error_check_good txn2_begin [is_valid_txn $txn2 $dbenv] TRUE
# Execute op2
set op2result [do_op $op $args $txn2 $dbenv]
# End txn2
error_check_good txn2_end [$txn2 $end] 0
if {$result == 0} {
	error_check_good op2_should_succeed $op2result $result
} else {
	set error [extract_error $op2result]
	error_check_good op2_wrong_failure $error $result
}

# Close any open db handles.  We had to wait until now
# because you can't close a database inside a transaction.
set handles [berkdb handles]
foreach handle $handles {
	if {[string range $handle 0 1] == "db" } {
		error_check_good db_close [$handle close] 0
	}
}

# Close the env
error_check_good dbenv_close [$dbenv close] 0
puts "\tFopscript completed successfully"

