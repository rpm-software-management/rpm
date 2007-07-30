# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001,2007 Oracle.  All rights reserved.
#
# $Id: rep008.tcl,v 12.15 2007/05/17 18:17:21 bostic Exp $
#
# TEST	rep008
# TEST	Replication, back up and synchronizing
# TEST
# TEST	Run a modified version of test001 in a replicated master
# TEST	environment.
# TEST	Close master and client.
# TEST	Copy the master log to the client.
# TEST	Clean the master.
# TEST	Reopen the master and client.
proc rep008 { method { niter 10 } { tnum "008" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}

	# Run for btree only.
	if { $checking_valid_methods } {
		set test_methods { btree }
		return $test_methods
	}
	if { [is_btree $method] == 0 } {
		puts "Rep$tnum: Skipping for method $method."
		return
	}

	# This test depends on copying logs, so can't be run with
	# in-memory logging.
	global mixed_mode_logging
	if { $mixed_mode_logging > 0 } {
		puts "Rep$tnum: Skipping for mixed-mode logging."
		return
	}

	set args [convert_args $method $args]

	# Run the body of the test with and without recovery.
	foreach r $test_recopts {
		puts "Rep$tnum ($method $r):\
		    Replication backup and synchronizing."
		rep008_sub $method $niter $tnum $r $args
	}
}

proc rep008_sub { method niter tnum recargs largs } {
	global testdir
	global util_path
	global rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create -txn nosync $verbargs \
	    -home $masterdir -errpfx MASTER \
	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create -txn nosync $verbargs \
	    -home $clientdir -errpfx CLIENT \
	    -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Run a modified test001 in the master (and update client).
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval test001 $method $niter 0 0 $tnum -env $masterenv $largs
	process_msgs $envlist

	puts "\tRep$tnum.b: Close client and master.  Copy logs."
	error_check_good client_close [$clientenv close] 0
	error_check_good master_close [$masterenv close] 0
	file copy -force $masterdir/log.0000000001 $testdir/log.save

	puts "\tRep$tnum.c: Clean master and reopen"
	#
	# Add sleep calls to ensure master's new log doesn't match
	# its old one in the ckp timestamp.
	#
	tclsleep 1
	env_cleanup $masterdir
	tclsleep 1
	env_cleanup $clientdir
	file copy -force $testdir/log.save $clientdir/log.0000000001
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Process the messages to get them out of the db.
	#
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist 0 NONE err
	error_check_bad err $err 0
	error_check_good errchk [is_substr $err "DB_REP_JOIN_FAILURE"] 1

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
