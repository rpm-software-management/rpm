# See the file LICENSE for redistribution information.
#
# Copyright (c) 2007 Oracle.  All rights reserved.
#
# $Id: rep073.tcl,v 1.3 2007/05/17 18:17:21 bostic Exp $
#
# TEST	rep073
# TEST
# TEST	Test of allowing clients to create and update their own scratch
# TEST	databases within the environment.  Doing so requires the use
# TEST	use of the DB_TXN_NOT_DURABLE flag for those databases.
#
proc rep073 { method { niter 200 } { tnum "073" } args } {

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
	set logsets [create_logsets 2]

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
			puts "Rep$tnum ($method $r $args):\
			    Test of non-durable databases and replication."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep073_sub $method $niter $tnum $l $r $args
		}
	}
}

proc rep073_sub { method niter tnum logset recargs largs } {
	source ./include.tcl
	global testdir
	global util_path
	global rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	env_cleanup $testdir

	set omethod [convert_method $method]
	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]

	# In-memory logs cannot be used with -txn nosync.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
	    -errpfx MASTER \
	    -home $masterdir $verbargs -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
	    -errpfx CLIENT \
	    -home $clientdir $verbargs -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	set start 0
	eval rep_test $method $masterenv NULL $niter $start $start 0 0 $largs
	incr start $niter
	process_msgs $envlist

	set mtestfile "master.db"
	set ctestfile "client.db"
	puts "\tRep$tnum.b: Open non-durable databases on master and client."
	set mdb [berkdb_open -create -auto_commit \
	    -btree -env $masterenv -notdurable $mtestfile]
	set cdb [berkdb_open -create -auto_commit \
	    -btree -env $clientenv -notdurable $ctestfile]
	process_msgs $envlist
	#
	# Verify that neither file exists on the other site.
	#
	error_check_good master_not_on_client \
	    [file exists $clientdir/$mtestfile] 0
	error_check_good client_not_on_master \
	    [file exists $masterdir/$ctestfile] 0

	#
	# Now write to the master database, process messages and
	# make sure nothing get sent to the client.
	#
	puts "\tRep$tnum.c: Write to non-durable database on master."
	eval rep_test $method $masterenv $mdb $niter $start $start 0 0 $largs
	incr start $niter
	process_msgs $envlist
	error_check_good master_not_on_client \
	    [file exists $clientdir/$mtestfile] 0

	# Make sure client can write to its own database.
	puts "\tRep$tnum.d: Write to non-durable database on client."
	eval rep_test $method $clientenv $cdb $niter $start $start 0 0 $largs
	process_msgs $envlist

	error_check_good mdb_close [$mdb close] 0
	error_check_good cdb_close [$cdb close] 0

	rep_verify $masterdir $masterenv $clientdir $clientenv
	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0

	replclose $testdir/MSGQUEUEDIR
}
