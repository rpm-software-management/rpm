# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep009.tcl,v 11.3 2003/08/28 19:59:15 sandstro Exp $
#
# TEST  rep009
# TEST	Replication and DUPMASTERs
# TEST  Run test001 in a replicated environment.
# TEST
# TEST  Declare one of the clients to also be a master.
# TEST  Close a client, clean it and then declare it a 2nd master.
proc rep009 { method { niter 10 } { tnum "009" } args } {

	puts "Rep$tnum: Replication DUPMASTER test."

        if { [is_btree $method] == 0 } {
		puts "Rep009: Skipping for method $method."
		return
	}
	set largs $args
	rep009_body $method $niter $tnum 0 $largs
	rep009_body $method $niter $tnum 1 $largs

}

proc rep009_body { method niter tnum clean largs } {
	global testdir

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR.2

	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

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

	repladd 3
	set cl2_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
#	set cl2_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
#	    -home $clientdir2 -rep_transport \[list 3 replsend\] \
#	    -verbose {rep on}"
	set cl2env [eval $cl2_envcmd -rep_client]
	error_check_good client2_env [is_valid_env $cl2env] TRUE

	# Bring the clients online by processing the startup messages.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]
		incr nproced [replprocessqueue $cl2env 3]

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
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}
	puts "\tRep$tnum.b: Declare a client to be a master."
	if { $clean } {
		error_check_good clientenv_close [$clientenv close] 0
		env_cleanup $clientdir
		set clientenv [eval $cl_envcmd -rep_master]
		error_check_good client_env [is_valid_env $clientenv] TRUE
	} else {
		error_check_good client_master [$clientenv rep_start -master] 0
	}

	#
	# Process the messages to get them out of the db.
	#
	for { set i 1 } { $i <= 3 } { incr i } {
		set seen_dup($i) 0
	}
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1 0 he nm dup1 err1]
		incr nproced [replprocessqueue $clientenv 2 0 he nm dup2 err2]
		incr nproced [replprocessqueue $cl2env 3 0 he nm dup3 err3]
		if { $dup1 != 0 } {
			set seen_dup(1) 1
			error_check_good downgrade1 \
			    [$masterenv rep_start -client] 0
		}
		if { $dup2 != 0 } {
			set seen_dup(2) 1
			error_check_good downgrade1 \
			    [$clientenv rep_start -client] 0
		}
		#
		# We might get errors after downgrading as the former
		# masters might get old messages from other clients.
		# If we get an error make sure it is after downgrade.
		if { $err1 != 0 } {
			error_check_good seen_dup1_err $seen_dup(1) 1
			error_check_good err1str [is_substr \
			    $err1 "invalid argument"] 1
		}
		if { $err2 != 0 } {
			error_check_good seen_dup2_err $seen_dup(2) 1
			error_check_good err2str [is_substr \
			    $err2 "invalid argument"] 1
		}
		#
		# This should never happen.  We'll check below.
		#
		if { $dup3 != 0 } {
			set seen_dup(3) 1
		}

		if { $nproced == 0 } {
			break
		}
	}
	error_check_good seen_dup1 $seen_dup(1) 1
	error_check_good seen_dup2 $seen_dup(2) 1
	error_check_bad seen_dup3 $seen_dup(3) 1

	puts "\tRep$tnum.c: Close environments"
	error_check_good master_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	error_check_good cl2_close [$cl2env close] 0
	replclose $testdir/MSGQUEUEDIR
}
