# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep008.tcl,v 1.3 2003/08/28 19:59:15 sandstro Exp $
#
# TEST  rep008
# TEST	Replication, back up and synchronizing
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  Close master and client.
# TEST  Copy the master log to the client.
# TEST  Clean the master.
# TEST  Reopen the master and client.
proc rep008 { method { niter 10 } { tnum "008" } args } {
	global testdir

	puts "Rep$tnum: Replication backup and synchronizing"
	set largs $args

	env_cleanup $testdir

        if { [is_btree $method] == 0 } {
		puts "Rep008: Skipping for method $method."
		return
	}

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
#	    -verbose {rep on} \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
#	    -verbose {rep on} \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the clients online by processing the startup messages.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Run a modified test001 in the master (and update client).
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval test001 $method $niter 0 0 $tnum -env $masterenv $largs
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}
	puts "\tRep$tnum.b: Close client and master.  Copy logs."
	error_check_good client_close [$clientenv close] 0
	error_check_good master_close [$masterenv close] 0
	file copy -force $masterdir/log.0000000001 $testdir/log.save

	puts "\tRep$tnum.c: Clean master and reopen"
	env_cleanup $masterdir
	env_cleanup $clientdir
	file copy -force $testdir/log.save $clientdir/log.0000000001
	set masterenv [eval $ma_envcmd -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	set clientenv [eval $cl_envcmd -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	#
	# Process the messages to get them out of the db.
	#
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
