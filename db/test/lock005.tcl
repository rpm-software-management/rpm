# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2001
#	Sleepycat Software.  All rights reserved.
#
# Id: lock005.tcl,v 1.1 2001/11/16 20:13:04 sandstro Exp 
#
# TEST	lock005
# TEST	Check that page locks are being released properly.

proc lock005 { } {
	source ./include.tcl

	puts "Lock005: Page lock release test"

	# Clean up after previous runs
	env_cleanup $testdir

	# Open/create the lock region
	set e [berkdb env -create -lock -home $testdir -txn -log]
	error_check_good env_open [is_valid_env $e] TRUE

	# Open/create the database
	set db [berkdb open -create -env $e -len 10 -queue q.db]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Start the first transaction
	set txn1 [$e txn -nowait]
	error_check_good txn_begin [is_valid_txn $txn1 $e] TRUE
	set ret [$db put -txn $txn1 -append record1]
	error_check_good dbput_txn1 $ret 1

	# Start a second transaction while first is still running
	set txn2 [$e txn -nowait]
	error_check_good txn_begin [is_valid_txn $txn2 $e] TRUE	
	set ret [catch {$db put -txn $txn2 -append record2} res] 
	error_check_good dbput_txn2 $ret 0
      error_check_good dbput_txn2recno $res 2	

	# Clean up
	error_check_good txn1commit [$txn1 commit] 0
	error_check_good txn2commit [$txn2 commit] 0
	error_check_good db_close [$db close] 0
	error_check_good env_close [$e close] 0

}

