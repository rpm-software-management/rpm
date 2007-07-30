# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001,2007 Oracle.  All rights reserved.
#
# $Id: rep078.tcl,v 12.2 2007/07/02 15:56:56 sue Exp $
#
# TEST  rep078
# TEST
# TEST	Replication and basic lease test.
# TEST	Set leases on master and 2 clients.
# TEST	Do a lease operation and process to all clients.
# TEST	Read with lease on master.  Do another lease operation
# TEST	and don't process on any client.  Try to read with
# TEST	on the master and verify it fails.  Process the messages
# TEST	to the clients and retry the read.
#
proc rep078 { method { tnum "078" } args } {
	source ./include.tcl

	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win9x platform."
		return
	}

	# Valid for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]
	set logsets [create_logsets 3]

	# Run the body of the test with and without recovery,
	# and with and without cleaning.  Skip recovery with in-memory
	# logging - it doesn't make sense.
	foreach r $test_recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Skipping rep$tnum for -recover\
				    with in-memory logs."
				continue
			}
			puts "Rep$tnum ($method $r):\
			    Replication and basic master leases."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			puts "Rep$tnum: Client 2 logs are [lindex $l 2]"
			rep078_sub $method $tnum $l $r $args
		}
	}
}

proc rep078_sub { method tnum logset recargs largs } {
	global testdir
	global rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	env_cleanup $testdir

	set qdir $testdir/MSGQUEUEDIR
	replsetup $qdir

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR2

	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]
	set c2_logtype [lindex $logset 2]

	# In-memory logs cannot be used with -txn nosync.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]
	set c2_logargs [adjust_logargs $c2_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]
	set c2_txnargs [adjust_txnargs $c2_logtype]

	# Set leases for 3 sites, 1 second timeout, 1% clock skew
	set nsites 3
	set lease_to 1000000
	set lease_tosec [expr $lease_to / 1000000]
	set lease_skew 0
	#
	# Since we have to use elections, the election code
	# assumes a 2-off site id scheme.
	# Open a master.
	repladd 2
	set err_cmd(0) "none"
	set crash(0) 0
	set pri(0) 100
	set envcmd(0) "berkdb_env -create $m_txnargs $m_logargs \
	    $verbargs -errpfx MASTER -home $masterdir \
	    -rep_lease \[list $nsites $lease_to $lease_skew\] \
	    -event rep_event \
	    -rep_client -rep_transport \[list 2 replsend\]"
	set masterenv [eval $envcmd(0) $recargs]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open two clients.
	repladd 3
	set err_cmd(1) "none"
	set crash(1) 0
	set pri(1) 10
	set envcmd(1) "berkdb_env -create $c_txnargs $c_logargs \
	    $verbargs -errpfx CLIENT -home $clientdir \
	    -rep_lease \[list $nsites $lease_to $lease_skew\] \
	    -event rep_event \
	    -rep_client -rep_transport \[list 3 replsend\]"
	set clientenv [eval $envcmd(1) $recargs]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	repladd 4
	set err_cmd(2) "none"
	set crash(2) 0
	set pri(2) 10
	set envcmd(2) "berkdb_env -create $c2_txnargs $c2_logargs \
	    $verbargs -errpfx CLIENT2 -home $clientdir2 \
	    -rep_lease \[list $nsites $lease_to $lease_skew\] \
	    -event rep_event \
	    -rep_client -rep_transport \[list 4 replsend\]"
	set clientenv2 [eval $envcmd(2) $recargs]
	error_check_good client_env [is_valid_env $clientenv2] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 2} {$clientenv 3} {$clientenv2 4}"
	process_msgs $envlist

	#
	# Run election to get a master.  Leases prevent us from
	# simply assigning a master.
	#
	set msg "Rep$tnum.a"
	puts "\tRep$tnum.a: Run initial election."
	set nvotes $nsites
	set winner 0
	setpriority pri $nsites $winner
	set elector [berkdb random_int 0 2]
	#
	# Note we send in a 0 for nsites because we set nsites back
	# when we started running with leases.  Master leases require
	# that nsites be set before calling rep_start, and master leases
	# require that the nsites arg to rep_elect be 0.
	#
	run_election envcmd envlist err_cmd pri crash $qdir $msg \
	    $elector 0 $nvotes $nsites $winner 0

	puts "\tRep$tnum.b: Create and open master database."
	set testfile "test.db"
	set omethod [convert_method $method]
	set masterdb [eval {berkdb_open_noerr -env $masterenv -auto_commit \
	    -create -mode 0644} $largs $omethod $testfile]
	error_check_good dbopen [is_valid_db $masterdb] TRUE

	process_msgs $envlist

	#
	# Just use numeric key so we don't have to worry about method.
	#
	set key 1
	puts "\tRep$tnum.c: Put some data to master database."
	do_leaseop $masterenv $masterdb $method $key $envlist

	set uselease ""
	set ignorelease "-nolease"
	puts "\tRep$tnum.d.0: Read with leases."
	check_leaseget $masterdb $key $uselease 0
	puts "\tRep$tnum.d.1: Read ignoring leases."
	check_leaseget $masterdb $key $ignorelease 0
	#
	# This should fail because the lease is expired and all
	# attempts by master to refresh it will not be processed.
	#
	set sleep [expr $lease_tosec + 1]
	puts "\tRep$tnum.e.0: Sleep $sleep secs to expire leases and read again."
	tclsleep $sleep
	check_leaseget $masterdb $key $uselease REP_LEASE_EXPIRED
	puts "\tRep$tnum.e.1: Read ignoring leases."
	check_leaseget $masterdb $key $ignorelease 0
	incr key

	#
	# This will fail because when we try to read the data
	# none of the clients will have processed the messages.
	# Sending in the 0 to do_leaseop means we don't process
	# the messages there.
	#
	puts "\tRep$tnum.f: Put data to master, verify read fails."
	do_leaseop $masterenv $masterdb $method $key $envlist 0
	check_leaseget $masterdb $key $uselease REP_LEASE_EXPIRED
	puts "\tRep$tnum.f.0: Read ignoring leases."
	check_leaseget $masterdb $key $ignorelease 0

	puts "\tRep$tnum.g: Process messages to clients."
	process_msgs $envlist
	puts "\tRep$tnum.h: Verify read with leases now succeeds."
	check_leaseget $masterdb $key $uselease 0
	process_msgs $envlist

	rep_verify $masterdir $masterenv $clientdir $clientenv
	rep_verify $masterdir $masterenv $clientdir2 $clientenv2 0 1 0

	error_check_good dbclose [$masterdb close] 0
	error_check_good mclose [$masterenv close] 0
	error_check_good cclose [$clientenv close] 0
	error_check_good c2close [$clientenv2 close] 0
	replclose $testdir/MSGQUEUEDIR
}
