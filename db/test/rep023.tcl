# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep023.tcl,v 11.3 2004/09/22 18:01:06 bostic Exp $
#
# TEST	rep023
# TEST	Replication using two master handles.
# TEST
# TEST	Open two handles on one master env.  Create two
# TEST	databases, one through each master handle.  Process
# TEST	all messages through the first master handle.  Make
# TEST	sure changes made through both handles are picked
# TEST	up properly.
#
proc rep023 { method { niter 10 } { tnum "023" } args } {
	global is_hp_test

	# We can't open two envs on HP-UX, so just skip the
	# whole test since that is at the core of it.
	if { $is_hp_test == 1 } {
		puts "Rep$tnum: Skipping for HP-UX."
		return
	}
	set args [convert_args $method $args]
	set logsets [create_logsets 2]

	# Run the body of the test with and without recovery, and
	# with and without -rep_start.
	set recopts { "" "-recover" }
	foreach r $recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping\
				    for in-memory logs with -recover."
				continue
			}
			foreach startopt { 0 1 } {
				if { $startopt == 1 } {
					set msg "with rep_start"
				} else {
					set msg ""
				}
				puts "Rep$tnum ($method $r $msg):\
				    Replication and openfiles."
				puts "Rep$tnum: Master logs are [lindex $l 0]"
				puts "Rep$tnum: Client logs are [lindex $l 1]"
				rep023_sub $method $niter $tnum $l $r $startopt $args
			}
		}
	}
}

proc rep023_sub { method niter tnum logset recargs startopt largs } {
	global testdir
	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	file mkdir $masterdir
	file mkdir $clientdir

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]

	# In-memory logs require a large log buffer, and cannot
	# be used with -txn nosync.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]

	# Open 1st master.
	repladd 1
	set ma_envcmd "berkdb_env -create $m_txnargs \
	    $m_logargs -lock_max 2500 \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env -create $m_txnargs \
#	    $m_logargs -lock_max 2500 \
#	    -errpfx MASTER -verbose {rep on} \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv1 [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv1] TRUE

	# Open 2nd handle on master.  The master envs will share
	# the same envid.
	set masterenv2 [eval $ma_envcmd]
	error_check_good master_env [is_valid_env $masterenv2] TRUE
	if { $startopt == 1 } {
		error_check_good rep_start [$masterenv2 rep_start -master] 0
	}

	# Open a client.
	repladd 2
	set cl_envcmd "berkdb_env -create $c_txnargs \
	    $c_logargs -lock_max 2500 \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env -create $c_txnargs \
#	    $c_logargs -lock_max 2500 \
#	    -errpfx CLIENT1 -verbose {rep on} \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the clients online by processing the startup messages.
	# Process messages on the first masterenv handle, not the second.
	set envlist "{$masterenv1 1} {$clientenv 2}"
	process_msgs $envlist

	puts "\tRep$tnum.a: Create database using 1st master handle."
	# Create a database using the 1st master.
	set testfile1 "m1$tnum.db"
	set omethod [convert_method $method]
	set db1 [eval {berkdb_open_noerr -env $masterenv1 -auto_commit \
	     -create -mode 0644} $largs $omethod $testfile1]
	error_check_good dbopen [is_valid_db $db1] TRUE

	puts "\tRep$tnum.b: Create database using 2nd master handle."
	# Create a different database using the 2nd master.
	set testfile2 "m2$tnum.db"
	set db2 [eval {berkdb_open_noerr -env $masterenv2 -auto_commit \
	     -create -mode 0644} $largs $omethod $testfile2]
	error_check_good dbopen [is_valid_db $db2] TRUE

	puts "\tRep$tnum.c: Process messages."
	# Process messages.
	process_msgs $envlist

	puts "\tRep$tnum.d: Run rep_test in 1st master; process messages."
	eval rep_test $method $masterenv1 $db1 $niter 0 0
	process_msgs $envlist

	puts "\tRep$tnum.e: Run rep_test in 2nd master; process messages."
	eval rep_test $method $masterenv2 $db2 $niter 0 0
	process_msgs $envlist

	# Contents of the two databases should match.
	error_check_good db_compare [db_compare \
	    $db1 $db2 $masterdir/$testfile1 $masterdir/$testfile2] 0

	puts "\tRep$tnum.f: Close 2nd master."
	error_check_good db2 [$db2 close] 0
	error_check_good master2_close [$masterenv2 close] 0

	puts "\tRep$tnum.g: Run test in master again."
	eval rep_test $method $masterenv1 $db1 $niter $niter 0
	process_msgs $envlist

	puts "\tRep$tnum.h: Closing"
	error_check_good db1 [$db1 close] 0
	error_check_good env0_close [$masterenv1 close] 0
	error_check_good env2_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
	return
}
