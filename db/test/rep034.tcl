# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep034.tcl,v 1.3 2004/09/22 18:01:06 bostic Exp $
#
# TEST	rep034
# TEST	Test of client startup synchronization.
# TEST
# TEST	One master, two clients.
# TEST	Run rep_test.
# TEST	Close one client and change master to other client.
# TEST	Reopen closed client - enter startup.
# TEST	Run rep_test and we should see live messages and startup complete.
#
proc rep034 { method { niter 2 } { tnum "034" } args } {

	set args [convert_args $method $args]
	set logsets [create_logsets 3]

	# Run the body of the test with and without recovery.
	set recopts { "" "-recover" }
	set startup { "stat" "ret" }
	foreach r $recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping\
				    for in-memory logs with -recover."
				continue
			}
			foreach s $startup {
				puts "Rep$tnum ($method $r $s $args):\
				    Test of startup synchronization detection."
				puts "Rep$tnum: Master logs are [lindex $l 0]"
				puts "Rep$tnum: Client 0 logs are [lindex $l 1]"
				puts "Rep$tnum: Client 1 logs are [lindex $l 2]"
				rep034_sub $method $niter $tnum $l $r $s $args
			}
		}
	}
}

proc rep034_sub { method niter tnum logset recargs stup largs } {
	global testdir
	global util_path
	global startup_done

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR2

	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]
	set c2_logtype [lindex $logset 2]

	# In-memory logs require a large log buffer, and cannot
	# be used with -txn nosync.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]
	set c2_logargs [adjust_logargs $c2_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]
	set c2_txnargs [adjust_txnargs $c2_logtype]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
#	    -verbose {rep on} -errpfx MASTER \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
#	    -verbose {rep on} -errpfx CLIENT \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Open a client
	repladd 3
	set cl2_envcmd "berkdb_env_noerr -create $c2_txnargs $c2_logargs \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
#	set cl2_envcmd "berkdb_env_noerr -create $c2_txnargs $c2_logargs \
#	    -verbose {rep on} -errpfx CLIENT2 \
#	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
	set client2env [eval $cl2_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $client2env] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2} {$client2env 3}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs
	process_msgs $envlist

        puts "\tRep$tnum.b: Close client and run with new master."
	error_check_good client_close [$clientenv close] 0
	set envlist "{$masterenv 1} {$client2env 3}"

	error_check_good master_downgr [$masterenv rep_start -client] 0
	error_check_good cl2_upgr [$client2env rep_start -master] 0
	#
	# Just so that we don't get confused who is master/client.
	#
	set newmaster $client2env
	set newclient $masterenv
	process_msgs $envlist

	# Run rep_test in the master (don't update client).
	# Run with dropping all client messages via replclear.
	eval rep_test $method $newmaster NULL $niter 0 0 0 $largs
	process_msgs $envlist
	replclear 2

	puts "\tRep$tnum.c: Restart client"
	set startup_done 0
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE
	set envlist "{$newclient 1} {$clientenv 2} {$newmaster 3}"
	process_msgs $envlist

	puts "\tRep$tnum.d: Verify client in startup mode"
	set start [stat_field $clientenv rep_stat "Startup complete"]
	error_check_good start_incomplete $start 0

	puts "\tRep$tnum.e: Generate live message"
	eval rep_test $method $newmaster NULL $niter 0 0 0 $largs
	process_msgs $envlist

	if { $stup == "stat" } {
		puts "\tRep$tnum.f: Verify client completed startup via stat"
		set start [stat_field $clientenv rep_stat "Startup complete"]
		error_check_good start_complete $start 1
	} else {
		puts "\tRep$tnum.f: Verify client completed startup via return"
		error_check_good start_complete $startup_done 1
	}

	error_check_good masterenv_close [$newclient close] 0
	error_check_good clientenv_close [$clientenv close] 0
	error_check_good client2env_close [$newmaster close] 0
	replclose $testdir/MSGQUEUEDIR
}
