# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep034.tcl,v 12.13 2006/09/08 20:32:18 bostic Exp $
#
# TEST	rep034
# TEST	Test of client startup synchronization.
# TEST
# TEST	One master, two clients.
# TEST	Run rep_test.
# TEST	Close one client and change master to other client.
# TEST	Reopen closed client - enter startup.
# TEST	Run rep_test and we should see live messages and startup complete.
# TEST	Run the test with/without client-to-client synchronization.
#
proc rep034 { method { niter 2 } { tnum "034" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}

	# Valid for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]
	set logsets [create_logsets 3]

	# Run the body of the test with and without recovery.
	#
	# Test a couple sets of options.  Getting 'startup' from the stat
	# or return value is unrelated to servicing requests from anywhere
	# or from the master.  List them together to make the test shorter.
	# We don't need to test every combination.
	#
	set opts { {stat anywhere} {ret from_master} }
	foreach r $test_recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping\
				    for in-memory logs with -recover."
				continue
			}
			foreach s $opts {
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

proc rep034_sub { method niter tnum logset recargs opts largs } {
	global anywhere
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

	set stup [lindex $opts 0]
	set anyopt [lindex $opts 1]
        if { $anyopt == "anywhere" } {
		set anywhere 1
	} else {
		set anywhere 0
	}

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
	    -event rep_startup_event \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
#	    -verbose {rep on} -errpfx MASTER \
#	    -event rep_startup_event \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
	    -event rep_startup_event \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
#	    -event rep_startup_event \
#	    -verbose {rep on} -errpfx CLIENT \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Open a client
	repladd 3
	set cl2_envcmd "berkdb_env_noerr -create $c2_txnargs $c2_logargs \
	    -event rep_startup_event \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
#	set cl2_envcmd "berkdb_env_noerr -create $c2_txnargs $c2_logargs \
#	    -event rep_startup_event \
#	    -verbose {rep on} -errpfx CLIENT2 \
#	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
	set client2env [eval $cl2_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $client2env] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2} {$client2env 3}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
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
	eval rep_test $method $newmaster NULL $niter 0 0 0 0 $largs
	process_msgs $envlist
	replclear 2

	puts "\tRep$tnum.c: Restart client"
	set startup_done 0
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	#
	# Record the number of rerequests now because they can
	# happen during initial processing or later.
	#
	if { $anyopt == "anywhere" } {
		set clrereq1 [stat_field $clientenv rep_stat \
		    "Client rerequests"]
		set nclrereq1 [stat_field $newclient rep_stat \
		    "Client rerequests"]
	}
	set envlist "{$newclient 1} {$clientenv 2} {$newmaster 3}"
	process_msgs $envlist

	puts "\tRep$tnum.d: Verify client in startup mode"
	set start [stat_field $clientenv rep_stat "Startup complete"]
	error_check_good start_incomplete $start 0

	puts "\tRep$tnum.e: Generate live message"
	eval rep_test $method $newmaster NULL $niter 0 0 0 0 $largs
	process_msgs $envlist

	#
	# If we're running with 'anywhere' request servicing, then we
	# need to run several more iterations.  When the new master took over
	# the old master requested from the down client and its request
	# got erased via the replclear.  We need to generate enough
	# new message now that the 2nd client is online again for the
	# newclient to make its request again.
	#
	if { $anyopt == "anywhere" } {
		puts "\tRep$tnum.e.1: Generate messages for rerequest"
		set niter 50
		eval rep_test $method $newmaster NULL $niter 0 0 0 0 $largs
		process_msgs $envlist
		set clrereq2 [stat_field $clientenv rep_stat \
		    "Client rerequests"]
		set nclrereq2 [stat_field $newclient rep_stat \
		    "Client rerequests"]
		#
		# Verify that we had a rerequest.  The before/after
		# values should not be the same.
		#
		error_check_bad clrereq $clrereq1 $clrereq2
		error_check_bad nclrereq $nclrereq1 $nclrereq2
		puts "\tRep$tnum.e.2: Generate live messages again"
		set niter 5
		eval rep_test $method $newmaster NULL $niter 0 0 0 0 $largs
		process_msgs $envlist
	}

	if { $stup == "stat" } {
		puts "\tRep$tnum.f: Verify client completed startup via stat"
		set start [stat_field $clientenv rep_stat "Startup complete"]
		error_check_good start_complete $start 1
	} else {
		puts "\tRep$tnum.f: Verify client completed startup via event"
		error_check_good start_complete $startup_done 1
	}

	puts "\tRep$tnum.g: Check message handling of client."
	#
	# The newclient would have made requests of clientenv while it
	# was down and then after it was up.  Therefore, clientenv's
	# stats should show that it received the request.
	#
	set req3 [stat_field $clientenv rep_stat "Client service requests"]
	if { $anyopt == "anywhere" } {
		error_check_bad req.bad $req3 0
	} else {
		error_check_good req $req3 0
	}

	error_check_good masterenv_close [$newclient close] 0
	error_check_good clientenv_close [$clientenv close] 0
	error_check_good client2env_close [$newmaster close] 0
	replclose $testdir/MSGQUEUEDIR
	set anywhere 0
}
