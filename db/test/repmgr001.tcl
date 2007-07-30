# See the file LICENSE for redistribution information.
#
# Copyright (c) 2007 Oracle.  All rights reserved.
#
# $Id: repmgr001.tcl,v 12.2 2007/05/17 18:17:21 bostic Exp $
#
# TEST	repmgr001
# TEST	Check repmgr stats.
# TEST
# TEST	Run for btree only because access method shouldn't matter.
# TEST
proc repmgr001 { method { niter 20 } { tnum "001" } args } {

	source ./include.tcl

	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win9x platform."
		return
	}

	# Skip for all methods except btree.
	if { $checking_valid_methods } {
		return btree
	}
	if { [is_btree $method] == 0 } {
		puts "Rep$tnum: skipping for non-btree method $method."
		return
	}

	set args [convert_args $method $args]

	puts "Rep$tnum ($method): Test of repmgr stats"
	repmgr001_sub $method $niter $tnum $args
}

proc repmgr001_sub { method niter tnum largs } {
	global testdir rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	env_cleanup $testdir
	set ports [available_ports 2]

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	# Open a master.
	puts "\tRepmgr$tnum.a: Start a master."
	set ma_envcmd "berkdb_env_noerr -create $verbargs -errpfx MASTER \
	    -home $masterdir -txn -rep"
	set masterenv [eval $ma_envcmd]
	$masterenv repmgr -ack all -nsites 2 \
	    -local [list localhost [lindex $ports 0]] \
	    -start master

	# Open a client
	puts "\tRepmgr$tnum.b: Start a client."
	set cl_envcmd "berkdb_env_noerr -create $verbargs -errpfx CLIENT \
	    -home $clientdir -txn -rep"
	set clientenv [eval $cl_envcmd]
	$clientenv repmgr -ack all -nsites 2 \
	    -local [list localhost [lindex $ports 1]] \
	    -remote [list localhost [lindex $ports 0]] \
	    -start client
	await_startup_done $clientenv

	puts "\tRepmgr$tnum.c: Run some transactions at master."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs

	error_check_good perm_no_failed_stat \
	    [stat_field $masterenv repmgr_stat "Acknowledgement failures"] 0

	error_check_good no_connections_dropped \
	    [stat_field $masterenv repmgr_stat "Connections dropped"] 0

	$clientenv close

	# Just do a few transactions (i.e., 3 of them), because each one is
	# expected to time out, and if we did many the test would take a long
	# time (with no benefit).
	#
	puts "\tRepmgr$tnum.d: Run transactions with no client."
	eval rep_test $method $masterenv NULL 3 $niter $niter 0 0 $largs

	error_check_bad perm_failed_stat \
	    [stat_field $masterenv repmgr_stat "Acknowledgement failures"] 0

	error_check_good connections_dropped \
	    [stat_field $masterenv repmgr_stat "Connections dropped"] 1

	# Bring the client back up, and down, a couple times, to test resetting
	# of stats.
	#
	puts "\tRepmgr$tnum.e: Shut down client (pause), check dropped connection."
	set clientenv [eval $cl_envcmd]
	$clientenv repmgr -ack all -nsites 2 \
	    -local [list localhost [lindex $ports 1]] \
	    -remote [list localhost [lindex $ports 0]] \
	    -start client
	await_startup_done $clientenv
	$clientenv close

	# Wait a moment for the dust to settle.
	tclsleep 5

	error_check_good connections_dropped \
	    [stat_field $masterenv repmgr_stat "Connections dropped"] 2
	$masterenv repmgr_stat -clear

	puts "\tRepmgr$tnum.e: Shut down, pause, check dropped connection (reset)."
	set clientenv [eval $cl_envcmd]
	$clientenv repmgr -ack all -nsites 2 \
	    -local [list localhost [lindex $ports 1]] \
	    -remote [list localhost [lindex $ports 0]] \
	    -start client
	await_startup_done $clientenv
	$clientenv close
	tclsleep 5

	error_check_good connections_dropped \
	    [stat_field $masterenv repmgr_stat "Connections dropped"] 1

	$masterenv close
}
