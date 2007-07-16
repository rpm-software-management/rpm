# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep039.tcl,v 1.15 2006/08/24 14:46:37 bostic Exp $
#
# TEST	rep039
# TEST	Test of internal initialization and master changes.
# TEST
# TEST	One master, two clients.
# TEST	Generate several log files. Remove old master log files.
# TEST	Delete client files and restart client.
# TEST	While initialization is happening, change masters.
# TEST	Vary the number of times we process messages to make sure
# TEST	we change masters at varying stages of the first internal
# TEST	initialization.
# TEST
# TEST	Run for btree only because of the number of permutations.
# TEST
proc rep039 { method { niter 200 } { tnum "039" } args } {

	source ./include.tcl

	# Run for btree and queue methods only.
	if { $checking_valid_methods } {
		set test_methods {}
		foreach method $valid_methods {
			if { [is_btree $method] == 1 || \
			    [is_queue $method] == 1 } {
				lappend test_methods $method
			}
		}
		return $test_methods
	}
	if { [is_btree $method] == 0 && [is_queue $method] == 0 } {
		puts "Rep$tnum: skipping for non-btree, non-queue method."
		return
	}

	# Skip for mixed-mode logging -- this test has a very large
	# set of iterations already.
	global mixed_mode_logging
	if { $mixed_mode_logging > 0 } {
		puts "Rep$tnum: Skipping for mixed mode logging."
		return
	}

	# This test needs to set its own pagesize.
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Rep$tnum: skipping for specific pagesizes"
		return
	}

	set args [convert_args $method $args]

	# Run the body of the test with and without recovery,
	# and with and without cleaning.
	set cleanopts { noclean clean }
	set archopts { archive noarchive }
	set nummsgs 5
	foreach r $test_recopts {
		foreach c $cleanopts {
			foreach a $archopts {
				for { set i 1 } { $i < $nummsgs } { incr i } {
					puts "Rep$tnum ($method $r $c $a $args):\
					    Test of internal init. $i message iters."
					rep039_sub $method $niter $tnum \
					    $r $c $a $i $args
				}
			}
		}
	}
}

proc rep039_sub { method niter tnum recargs clean archive pmsgs largs } {
	global testdir
	global util_path

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR2

	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	# Log size is small so we quickly create more than one.
	# The documentation says that the log file must be at least
	# four times the size of the in-memory log buffer.
	set pagesize 4096
	append largs " -pagesize $pagesize "
	set log_buf [expr $pagesize * 2]
	set log_max [expr $log_buf * 4]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -verbose {rep on} -errpfx MASTER \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -verbose {rep on} -errpfx CLIENT \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Open 2nd client
	repladd 3
	set cl2_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
#	set cl2_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -verbose {rep on} -errpfx CLIENT2 \
#	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
	set clientenv2 [eval $cl2_envcmd $recargs -rep_client]
	error_check_good client2_env [is_valid_env $clientenv2] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2} {$clientenv2 3}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
	process_msgs $envlist

	puts "\tRep$tnum.b: Close client."
	error_check_good client_close [$clientenv close] 0

	set res [eval exec $util_path/db_archive -l -h $clientdir]
	set last_client_log [lindex [lsort $res] end]

	set stop 0
	while { $stop == 0 } {
		# Run rep_test in the master (don't update client).
		puts "\tRep$tnum.c: Running rep_test in replicated env."
		eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
		#
		# Clear messages for first client.  We want that site
		# to get far behind.
		#
		replclear 2
		puts "\tRep$tnum.d: Run db_archive on master."
		set res [eval exec $util_path/db_archive -d -h $masterdir]
		set res [eval exec $util_path/db_archive -l -h $masterdir]
		if { [lsearch -exact $res $last_client_log] == -1 } {
			set stop 1
		}
	}

	set envlist "{$masterenv 1} {$clientenv2 3}"
	process_msgs $envlist

	if { $archive == "archive" } {
		puts "\tRep$tnum.d: Run db_archive on client2."
		set res [eval exec $util_path/db_archive -l -h $clientdir2]
		error_check_bad \
		    log.1.present [lsearch -exact $res log.0000000001] -1
		set res [eval exec $util_path/db_archive -d -h $clientdir2]
		set res [eval exec $util_path/db_archive -l -h $clientdir2]
		error_check_good \
		    log.1.gone [lsearch -exact $res log.0000000001] -1
	} else {
		puts "\tRep$tnum.d: Skipping db_archive on client2."
	}

	puts "\tRep$tnum.e: Reopen client ($clean)."
	if { $clean == "clean" } {
		env_cleanup $clientdir
	}
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE
	set envlist "{$masterenv 1} {$clientenv 2} {$clientenv2 3}"
	proc_msgs_once $envlist

	#
	# We want to simulate a master continually getting new
	# records while an update is going on.
	#
	set entries 10
	eval rep_test $method $masterenv NULL $entries $niter 0 0 0 $largs
	#
	# We call proc_msgs_once N times to get us into page recovery:
	# 1.  Send master messages and client finds master.
	# 2.  Master replies and client does verify.
	# 3.  Master gives verify_fail and client does update_req.
	# 4.  Master send update info and client does page_req.
	#
	# We vary the number of times we call proc_msgs_once (via pmsgs)
	# so that we test switching master at each point in the
	# internal initialization processing.
	#
	set nproced 0
	puts "\tRep$tnum.f: Get partially through initialization ($pmsgs iters)"
	for { set i 1 } { $i < $pmsgs } { incr i } {
		incr nproced [proc_msgs_once $envlist]
	}
	replclear 1
	replclear 3
	puts "\tRep$tnum.g: Downgrade/upgrade master."
	error_check_good downgrade [$masterenv rep_start -client] 0
	error_check_good upgrade [$clientenv2 rep_start -master] 0
	process_msgs $envlist
	#
	# Now simulate continual updates to the new master.  Each
	# time through we just process messages once before
	# generating more updates.
	#
	set niter 10
	for { set i 0 } { $i < $niter } { incr i } {
		set nproced 0
		set start [expr $i * $entries]
		eval rep_test $method $clientenv2 NULL $entries $start \
		    $start 0 0 $largs
		incr nproced [proc_msgs_once $envlist]
		error_check_bad nproced $nproced 0
	}
	set start [expr $i * $entries]
	process_msgs $envlist

	puts "\tRep$tnum.h: Verify logs and databases"
	rep_verify $clientdir2 $clientenv2 $masterdir $masterenv 1

	# Process messages again in case we are running with debug_rop.
	process_msgs $envlist
	rep_verify $clientdir2 $clientenv2 $clientdir $clientenv 1

	# Add records to the master and update client.
	puts "\tRep$tnum.i: Add more records and check again."
	set entries 10
	eval rep_test $method $clientenv2 NULL $entries $start \
	    $start 0 0 $largs
	process_msgs $envlist 0 NONE err

	# Check again that everyone is identical.
	rep_verify $clientdir2 $clientenv2 $masterdir $masterenv 1
	process_msgs $envlist
	rep_verify $clientdir2 $clientenv2 $clientdir $clientenv 1

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	error_check_good clientenv2_close [$clientenv2 close] 0
	replclose $testdir/MSGQUEUEDIR
}
