# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep008.tcl,v 1.11 2004/09/22 18:01:06 bostic Exp $
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
	global mixed_mode_logging

	if { $mixed_mode_logging == 1 } {
		puts "Rep$tnum: Skipping for mixed-mode logging."
		return
	}
	if { [is_btree $method] == 0 } {
		puts "Rep$tnum: Skipping for method $method."
		return
	}

	set args [convert_args $method $args]

	# Run the body of the test with and without recovery.
	set recopts { "" "-recover" }
	foreach r $recopts {
		puts "Rep$tnum ($method $r):\
		    Replication backup and synchronizing."
		rep008_sub $method $niter $tnum $r $args
	}
}

proc rep008_sub { method niter tnum recargs largs } {
	global testdir
	global util_path

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
#	    -verbose {rep on} -errpfx MASTER \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
#	    -verbose {rep on} -errpfx CLIENT \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

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

	#
	# Process the messages to get them out of the db.
	#
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist 0 NONE err
	error_check_bad err $err 0
	error_check_good errchk [is_substr $err "Client was never part"] 1

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
