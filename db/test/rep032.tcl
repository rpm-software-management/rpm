# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep032.tcl,v 1.3 2004/09/22 18:01:06 bostic Exp $
#
# TEST	rep032
# TEST	Test of log gap processing.
# TEST
# TEST	One master, one clients.
# TEST	Run rep_test.
# TEST	Run rep_test without sending messages to client.
# TEST  Make sure client missing the messages catches up properly.
#
proc rep032 { method { niter 200 } { tnum "032" } args } {
	set args [convert_args $method $args]
	set logsets [create_logsets 2]

	# Run the body of the test with and without recovery.
	set recopts { "" "-recover" }
	foreach r $recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping\
				    for in-memory logs with -recover."
				continue
			}
			puts "Rep$tnum ($method $r $args):\
			    Test of log gap processing."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep032_sub $method $niter $tnum $l $r $args
		}
	}
}

proc rep032_sub { method niter tnum logset recargs largs } {
	global testdir
	global util_path

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

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs
	process_msgs $envlist

	puts "\tRep$tnum.b: Check client processed everything properly."
	set queued [stat_field $clientenv rep_stat "Maximum log records queued"]
	set request [stat_field $clientenv rep_stat "Log records requested"]
	error_check_good queued $queued 0
	error_check_good request $request 0

	# Run rep_test in the master (don't update client).
	# First run with dropping all client messages via replclear.
	puts "\tRep$tnum.c: Running rep_test dropping client msgs."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs
	replclear 2
	process_msgs $envlist

	#
	# Need new operations to force log gap processing to
	# request missing pieces.
	#
	puts "\tRep$tnum.d: Running rep_test again replicated."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs
	process_msgs $envlist

	puts "\tRep$tnum.e: Check we re-requested and had a backlog."
	set queued [stat_field $clientenv rep_stat "Maximum log records queued"]
	set request [stat_field $clientenv rep_stat "Log records requested"]
	error_check_bad queued $queued 0
	error_check_bad request $request 0

	puts "\tRep$tnum.f: Verify logs and databases"
	# Check that master and client logs and dbs are identical.
	# Logs first ...
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $masterdir > $masterdir/prlog} result]
	error_check_good stat_mprlog $stat 0
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $clientdir > $clientdir/prlog} result]
	error_check_good stat_cprlog $stat 0
	error_check_good log_cmp \
	    [filecmp $masterdir/prlog $clientdir/prlog] 0

	# ... now the databases.
	set dbname "test.db"
	set db1 [eval {berkdb_open -env $masterenv} $largs {-rdonly $dbname}]
	set db2 [eval {berkdb_open -env $clientenv} $largs {-rdonly $dbname}]

	error_check_good comparedbs [db_compare \
	    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
