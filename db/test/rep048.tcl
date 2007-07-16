# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep048.tcl,v 12.8 2006/08/24 14:46:38 bostic Exp $
#
# TEST  rep048
# TEST	Replication and log gap bulk transfers.
# TEST	Have two master env handles.  Turn bulk on in
# TEST	one (turns it on for both).  Turn it off in the other.
# TEST	While toggling, send log records from both handles.
# TEST	Process message and verify master and client match.
#
proc rep048 { method { nentries 3000 } { tnum "048" } args } {
	source ./include.tcl

	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]

	# Run the body of the test with and without recovery.
	foreach r $test_recopts {
		puts "Rep$tnum ($method $r):\
		    Replication and toggling bulk transfer."
		rep048_sub $method $nentries $tnum $r $args
	}
}

proc rep048_sub { method niter tnum recargs largs } {
	source ./include.tcl
	global overflowword1
	global overflowword2

	set orig_tdir $testdir
	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR
	set overflowword1 "0"
	set overflowword2 "0"

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	file mkdir $clientdir
	file mkdir $masterdir

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env -create -txn nosync \
	    -home $masterdir -rep_master -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env -create -txn nosync \
#	    -errpfx MASTER -verbose {rep on} \
#	    -home $masterdir -rep_master -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	repladd 2
	set cl_envcmd "berkdb_env -create -txn nosync -home $clientdir \
	    -rep_client -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env -create -txn nosync -home $clientdir \
#	    -errpfx CLIENT -verbose {rep on} \
#	    -rep_client -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the client online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	puts "\tRep$tnum.a: Create and open master databases"
	set testfile "test$tnum.db"
	set omethod [convert_method $method]
	set masterdb [eval {berkdb_open_noerr -env $masterenv -auto_commit \
	    -create -mode 0644} $largs $omethod $testfile]
	error_check_good dbopen [is_valid_db $masterdb] TRUE

	set scrlog $testdir/repscript.log
	puts "\tRep$tnum.b: Fork child process."
	set pid [exec $tclsh_path $test_path/wrap.tcl \
	    rep048script.tcl $scrlog \
	    $masterdir &]

	# Wait for child process to start up.
	while { 1 } {
		if { [file exists $masterdir/marker.file] == 0  } {
			tclsleep 1
		} else {
			tclsleep 1
			break
		}
	}
	# Run a modified test001 in the master (and update clients).
	# Call it several times so make sure that we get descheduled.
	puts "\tRep$tnum.c: Basic long running txn"
	set div 10
	set loop [expr $niter / $div]
	set start 0
	for { set i 0 } { $i < $div } {incr i} {
		rep_test_bulk $method $masterenv $masterdb $loop $start $start 0
		process_msgs $envlist
		set start [expr $start + $loop]
		tclsleep 1
	}
	error_check_good dbclose [$masterdb close] 0
	set marker [open $masterdir/done.file w]
	close $marker

	set bulkxfer1 [stat_field $masterenv rep_stat "Bulk buffer transfers"]
	error_check_bad bulk $bulkxfer1 0

	puts "\tRep$tnum.d: Waiting for child ..."
	# Watch until the child is done.
	watch_procs $pid 5
	process_msgs $envlist
	rep_verify $masterdir $masterenv $clientdir $clientenv 0 1 1 $testfile
	rep_verify $masterdir $masterenv $clientdir $clientenv 0 1 0 "child.db"

	error_check_good mclose [$masterenv close] 0
	error_check_good cclose [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
