# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2001
#	Sleepycat Software.  All rights reserved.
#
# Id: recd015.tcl,v 1.3 2001/05/05 14:08:14 margo Exp 
#
# Recovery Test 15.
# This is a recovery test for testing lots of prepared txns.
# This test is to force the use of txn_recover to call with the
# DB_FIRST flag and then DB_NEXT.
proc recd015 { method args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Recd015: $method ($args) large number of prepared txns test"

	# Create the database and environment.
	env_cleanup $testdir

	set numtxns 250
	set testfile recd015.db
	set txnmax [expr $numtxns + 5]

	puts "\tRecd015.a: Executing child proc to prepare $numtxns txns"
	set env_cmd "berkdb env -create -txn_max $txnmax -txn -home $testdir"
	set env [eval $env_cmd]
	error_check_good dbenv [is_valid_env $env] TRUE
	set db [eval {berkdb_open -create} $omethod -env $env $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0
	error_check_good envclose [$env close] 0

	set gidf $testdir/gidfile

	fileremove -f $gidf
	set proclist {}
	set p [exec $tclsh_path $test_path/wrap.tcl recd15script.tcl \
	    $testdir/recdout $env_cmd $testfile $gidf $numtxns &]

	lappend proclist $p
	watch_procs 5
	set f1 [open $testdir/recdout r]
	set r [read $f1]
	puts $r
	close $f1
	fileremove -f $testdir/recdout


	berkdb debug_check
	puts -nonewline "\tRecd15.d: Running recovery ... "
	flush stdout
	berkdb debug_check
	set env_cmd \
	    "berkdb env -recover -create -txn_max $txnmax -txn -home $testdir"
	set env [eval $env_cmd]
	error_check_good dbenv-recover [is_valid_env $env] TRUE
	puts "complete"

	set txnlist [$env txn_recover]
	set gfd [open $gidf r]
	set i 0
	while { [gets $gfd gid] != -1 } {
		set gids($i) $gid
		incr i
	}
	close $gfd
	#
	# Make sure we have as many as we expect
	error_check_good num_gids $i $numtxns

	#
	# Note that this assumes the txn_recover gives them back to
	# us in the order we prepared them.  If that is an invalid
	# assumption we can spit out the GIDs below to a new file,
	# sort them both to temp files and compare that way.
	set i 0
	puts "\tRecd15.e: Comparing GIDs "
	foreach tpair $txnlist {
		set txn [lindex $tpair 0]
		set gid [lindex $tpair 1]
		error_check_good gidcompare $gid $gids($i)
		error_check_good txnabort [$txn abort] 0
		incr i
	}
	set stat [catch {exec $util_path/db_printlog -h $testdir \
	    > $testdir/LOG } ret]
	error_check_good db_printlog $stat 0
	fileremove $testdir/LOG
}

