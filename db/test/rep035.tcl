# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep035.tcl,v 11.3 2004/09/22 18:01:06 bostic Exp $
#
# TEST  	rep035
# TEST	Test sync-up recovery in replication.
# TEST
# TEST	We need to fork off 3 child tclsh processes to operate
# TEST	on Site 3's (client always) home directory:
# TEST	Process 1 continually calls lock_detect.
# TEST	Process 2 continually calls txn_checkpoint.
# TEST	Process 3 continually calls memp_trickle.
# TEST	Process 4 continually calls log_archive.
# TEST	Sites 1 and 2 will continually swap being master
# TEST	(forcing site 3 to continually run sync-up recovery)
# TEST 	New master performs 1 operation, replicates and downgrades.

proc rep035 { method { niter 100 } { tnum "035" } args } {
	global passwd
	global has_crypto

	set saved_args $args
	set logsets [create_logsets 3]

	foreach l $logsets {
		set envargs ""
		set args $saved_args
		puts "Rep$tnum: Test sync-up recovery ($method)."
		puts "Rep$tnum: Master logs are [lindex $l 0]"
		puts "Rep$tnum: Client 0 logs are [lindex $l 1]"
		puts "Rep$tnum: Client 1 logs are [lindex $l 2]"
		rep035_sub $method $niter $tnum $envargs $l $args
	}
}

proc rep035_sub { method niter tnum envargs logset args } {
	source ./include.tcl
	global testdir
	global encrypt

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir1 $testdir/CLIENTDIR1
	set clientdir2 $testdir/CLIENTDIR2

	file mkdir $masterdir
	file mkdir $clientdir1
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
	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 $envargs -home $masterdir $m_logargs \
	    -errpfx MASTER -errfile /dev/stderr $m_txnargs -rep_master \
	    -rep_transport \[list 1 replsend\]"
#	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
#	    -log_max 1000000 $envargs -home $masterdir $m_logargs \
#	    -errpfx MASTER -errfile /dev/stderr $m_txnargs -rep_master \
#	    -verbose {rep on} \
#	    -rep_transport \[list 1 replsend\]"
	set env1 [eval $env_cmd(M)]
	error_check_good env1 [is_valid_env $env1] TRUE

	# Open two clients
	repladd 2
	set env_cmd(C1) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 $envargs -home $clientdir1 $c_logargs \
	    -errfile /dev/stderr -errpfx CLIENT $c_txnargs -rep_client \
	    -rep_transport \[list 2 replsend\]"
#	set env_cmd(C1) "berkdb_env_noerr -create -lock_max 2500 \
#	    -log_max 1000000 $envargs -home $clientdir1 $c_logargs \
#	    -errfile /dev/stderr -errpfx CLIENT $c_txnargs -rep_client \
#	    -verbose {rep on} \
#	    -rep_transport \[list 2 replsend\]"
	set env2 [eval $env_cmd(C1)]
	error_check_good env2 [is_valid_env $env2] TRUE

	# Second client needs lock_detect flag.
	repladd 3
	set env_cmd(C2) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 $envargs -home $clientdir2 $c2_logargs \
	    -errpfx CLIENT2 -errfile /dev/stderr $c2_txnargs -rep_client \
	    -lock_detect default -rep_transport \[list 3 replsend\]"
#	set env_cmd(C2) "berkdb_env_noerr -create -lock_max 2500 \
#	    -log_max 1000000 $envargs -home $clientdir2 $c2_logargs \
#	    -errpfx CLIENT2 -errfile /dev/stderr $c2_txnargs -rep_client \
#	    -verbose {rep on} \
#	    -lock_detect default -rep_transport \[list 3 replsend\]"
	set env3 [eval $env_cmd(C2)]
	error_check_good client_env [is_valid_env $env3] TRUE

	# Bring the client online by processing the startup messages.
	set envlist "{$env1 1} {$env2 2} {$env3 3}"
	process_msgs $envlist

	# We need to fork off 3 child tclsh processes to operate
	# on Site 3's (client always) home directory:
	#	Process 1 continually calls lock_detect (DB_LOCK_DEFAULT)
	#	Process 2 continually calls txn_checkpoint (DB_FORCE)
	#	Process 3 continually calls memp_trickle (large % like 90)
	# 	Process 4 continually calls log_archive.

	puts "\tRep$tnum.a: Fork child process running lock_detect on client2."
	set pid1 [exec $tclsh_path $test_path/wrap.tcl \
	    rep035script.tcl $testdir/lock_detect.log \
	    $clientdir2 detect &]

	puts "\tRep$tnum.b: Fork child process running txn_checkpoint on client2."
	set pid2 [exec $tclsh_path $test_path/wrap.tcl \
	    rep035script.tcl $testdir/txn_checkpoint.log \
	    $clientdir2 checkpoint &]

	puts "\tRep$tnum.c: Fork child process running memp_trickle on client2."
	set pid3 [exec $tclsh_path $test_path/wrap.tcl \
	    rep035script.tcl $testdir/memp_trickle.log \
	    $clientdir2 trickle &]

	puts "\tRep$tnum.d: Fork child process running log_archive on client2."
	set pid4 [exec $tclsh_path $test_path/wrap.tcl \
	    rep035script.tcl $testdir/log_archive.log \
	    $clientdir2 archive &]

	set pidlist [list $pid1 $pid2 $pid3 $pid4]

	#
	# Sites 1 and 2 will continually swap being master
	# forcing site 3 to continually run sync-up recovery.
	# New master performs 1 operation, replicates and downgrades.
	# Site 3 will always stay a client.
	#
	# Set up all the master/client data we're going to need
	# to keep track of and swap.  Set up the handles for rep_test.
	#

	set masterenv $env1
	set mid 1
	set clientenv $env2
	set cid 2
	set testfile "test$tnum.db"
	set args [convert_args $method]
	set omethod [convert_method $method]
	set mdb_cmd "{berkdb_open_noerr} -env $masterenv -auto_commit \
	     -create $omethod $args -mode 0644 $testfile"
	set cdb_cmd "{berkdb_open_noerr} -env $clientenv -auto_commit \
	     $omethod $args -mode 0644 $testfile"

	set masterdb [eval $mdb_cmd]
	error_check_good dbopen [is_valid_db $masterdb] TRUE
	process_msgs $envlist

	set clientdb [eval $cdb_cmd]
	error_check_good dbopen [is_valid_db $clientdb] TRUE

	tclsleep 2
	puts "\tRep$tnum.e: Swap master and client $niter times."
	for { set i 0 } { $i < $niter } { incr i } {

		# Do a few ops
		eval rep_test $method $masterenv $masterdb 2 $i $i
		set envlist "{$masterenv $mid} {$clientenv $cid} {$env3 3}"
		process_msgs $envlist

		# Do one op on master and process messages and drop
		# to clientenv to force sync-up recovery next time.
		eval rep_test $method $masterenv $masterdb 1 $i $i
		set envlist "{$masterenv $mid} {$env3 3}"
		replclear $cid
		process_msgs $envlist

		# Swap all the info we need.
		set tmp $masterenv
		set masterenv $clientenv
		set clientenv $tmp

		set tmp $masterdb
		set masterdb $clientdb
		set clientdb $tmp

		set tmp $mid
		set mid $cid
		set cid $tmp

		set tmp $mdb_cmd
		set mdb_cmd $cdb_cmd
		set cdb_cmd $tmp

		puts "\tRep$tnum.e.$i: Swap: master $mid, client $cid"
		error_check_good downgrade [$clientenv rep_start -client] 0
		error_check_good upgrade [$masterenv rep_start -master] 0
		set envlist "{$masterenv $mid} {$clientenv $cid} {$env3 3}"
		process_msgs $envlist

		# Close old and reopen since we will get HANDLE_DEAD
		# otherwise because we dropped messages to the new master.
		error_check_good masterdb [$masterdb close] 0
		error_check_good clientdb [$clientdb close] 0

		set masterdb [eval $mdb_cmd]
		error_check_good dbopen [is_valid_db $masterdb] TRUE

		set clientdb [eval $cdb_cmd]
		error_check_good dbopen [is_valid_db $clientdb] TRUE
		process_msgs $envlist
	}

	# Communicate with child processes by creating a marker file.
	set markerenv [berkdb_env -create -home $testdir -txn]
	error_check_good markerenv_open [is_valid_env $markerenv] TRUE
	set marker [eval "berkdb_open \
	    -create -btree -auto_commit -env $markerenv marker.db"]
	error_check_good marker_close [$marker close] 0

	# Script should be able to shut itself down fairly quickly.
	watch_procs $pidlist 5

	error_check_good masterdb [$masterdb close] 0
	error_check_good clientdb [$clientdb close] 0
	error_check_good env1_close [$env1 close] 0
	error_check_good env2_close [$env2 close] 0
	error_check_good env3_close [$env3 close] 0
	error_check_good markerenv_close [$markerenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
