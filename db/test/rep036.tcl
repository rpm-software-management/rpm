# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep036.tcl,v 11.2 2004/09/22 18:01:06 bostic Exp $
#
# TEST  	rep036
# TEST	Multiple master processes writing to the database.
# TEST	One process handles all message processing.

proc rep036 { method { niter 200 } { tnum "036" } args } {
	if { [is_btree $method] == 0 } {
		puts "Rep$tnum: Skipping for method $method."
		return
	}

	set saved_args $args
	set logsets [create_logsets 3]

	foreach l $logsets {
		set envargs ""
		set args $saved_args
		puts "Rep$tnum: Test sync-up recovery ($method)."
		puts "Rep$tnum: Master logs are [lindex $l 0]"
		puts "Rep$tnum: Client 0 logs are [lindex $l 1]"
		puts "Rep$tnum: Client 1 logs are [lindex $l 2]"
		rep036_sub $method $niter $tnum $envargs $l $args
	}
}

proc rep036_sub { method niter tnum envargs logset args } {
	source ./include.tcl
	global testdir

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]

	# In-memory logs require a large log buffer.
	# We always run this test with -txn, so don't adjust txnargs.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]

	# Open a master.
	repladd 1
	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 $envargs -home $masterdir $m_logargs \
	    -errpfx MASTER -errfile /dev/stderr -txn -rep_master \
	    -rep_transport \[list 1 replsend\]"
#	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
#	    -log_max 1000000 $envargs -home $masterdir $m_logargs \
#	    -errpfx MASTER -errfile /dev/stderr -txn -rep_master \
#	    -verbose {rep on} \
#	    -rep_transport \[list 1 replsend\]"
	set env1 [eval $env_cmd(M)]
	error_check_good env1 [is_valid_env $env1] TRUE

	# Open a client
	repladd 2
	set env_cmd(C) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 $envargs -home $clientdir $c_logargs \
	    -errfile /dev/stderr -errpfx CLIENT -txn -rep_client \
	    -rep_transport \[list 2 replsend\]"
#	set env_cmd(C) "berkdb_env_noerr -create -lock_max 2500 \
#	    -log_max 1000000 $envargs -home $clientdir $c_logargs \
#	    -errfile /dev/stderr -errpfx CLIENT -txn -rep_client \
#	    -verbose {rep on} \
#	    -rep_transport \[list 2 replsend\]"
	set env2 [eval $env_cmd(C)]
	error_check_good env2 [is_valid_env $env2] TRUE

	# Bring the client online by processing the startup messages.
	set envlist "{$env1 1} {$env2 2}"
	process_msgs $envlist

	# Set up master database.
	set testfile "rep$tnum.db"
	set omethod [convert_method $method]
	set mdb [eval {berkdb_open_noerr} -env $env1 -auto_commit \
	    -create -mode 0644 $omethod $testfile]
	error_check_good dbopen [is_valid_db $mdb] TRUE

	# Put a record in the master database.
	set key MAIN_KEY
	set string MAIN_STRING
	set t [$env1 txn]
	error_check_good txn [is_valid_txn $t $env1] TRUE
	set txn "-txn $t"

	set ret [eval \
	    {$mdb put} $txn {$key [chop_data $method $string]}]
	error_check_good mdb_put $ret 0
	error_check_good txn_commit [$t commit] 0

	# Fork two writers that write to the master.
	set pidlist {}
	foreach writer { 1 2 } {
		puts "\tRep$tnum.a: Fork child process WRITER$writer."
		set pid [exec $tclsh_path $test_path/wrap.tcl \
		    rep036script.tcl $testdir/rep036script.log.$writer \
		    $masterdir $writer $niter btree &]
		lappend pidlist $pid
	}

	# Run the main loop until the writers signal completion.
	set i 0
	while { [file exists $testdir/1.db] == 0 && \
	   [file exists $testdir/2.db] == 0 } {
		set string MAIN_STRING.$i

		set t [$env1 txn]
		error_check_good txn [is_valid_txn $t $env1] TRUE
		set txn "-txn $t"
		set ret [eval \
			{$mdb put} $txn {$key [chop_data $method $string]}]
		error_check_good mdb_put $ret 0
		error_check_good txn_commit [$t commit] 0

		if { [expr $i % 10] == 0 } {
			puts "\tRep036.c: Wrote MAIN record $i"
		}
		incr i

		# Process messages.
		process_msgs $envlist

		# Wait a while, then do it all again.
		tclsleep 1
	}


	# Confirm that the writers are done and process the messages
	# once more to be sure the client is caught up.
	watch_procs $pidlist 1
	process_msgs $envlist

	puts "\tRep$tnum.c: Verify logs and databases"
	# Check that master and client logs and dbs are identical.
	# Logs first ...
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $masterdir > $masterdir/prlog} result]
	error_check_good stat_mprlog $stat 0
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $clientdir > $clientdir/prlog} result]
	error_check_good mdb [$mdb close] 0
	error_check_good stat_cprlog $stat 0
#	error_check_good log_cmp \
#	    [filecmp $masterdir/prlog $clientdir/prlog] 0

	# ... now the databases.
	set db1 [eval {berkdb_open -env $env1 -rdonly $testfile}]
	set db2 [eval {berkdb_open -env $env2 -rdonly $testfile}]

	error_check_good comparedbs [db_compare \
	    $db1 $db2 $masterdir/$testfile $clientdir/$testfile] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0

	error_check_good env1_close [$env1 close] 0
	error_check_good env2_close [$env2 close] 0
	replclose $testdir/MSGQUEUEDIR
}
