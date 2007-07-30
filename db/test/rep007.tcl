# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001,2007 Oracle.  All rights reserved.
#
# $Id: rep007.tcl,v 12.16 2007/05/17 18:17:21 bostic Exp $
#
# TEST  	rep007
# TEST	Replication and bad LSNs
# TEST
# TEST	Run rep_test in a replicated master env.
# TEST  	Close the client.  Make additional changes to master.
# TEST  	Close the master.  Open the client as the new master.
# TEST 	Make several different changes.  Open the old master as
# TEST	the client.  Verify periodically that contents are correct.
proc rep007 { method { niter 10 } { tnum "007" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}

	# All access methods are allowed.
	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]
	set logsets [create_logsets 3]

	# Run the body of the test with and without recovery.
	foreach r $test_recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping for\
				    in-memory logs with -recover."
				continue
			}
			puts "Rep$tnum ($method $r): Replication and bad LSNs."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client1 logs are [lindex $l 1]"
			puts "Rep$tnum: Client2 logs are [lindex $l 2]"
			rep007_sub $method $niter $tnum $l $r $args
		}
	}
}

proc rep007_sub { method niter tnum logset recargs largs } {
	global testdir
	global rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	env_cleanup $testdir

	set orig_tdir $testdir
	set omethod [convert_method $method]

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR.2
	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	set m_logtype [lindex $logset 0]
	set m_logargs [adjust_logargs $m_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]

	set c_logtype [lindex $logset 1]
	set c_logargs [adjust_logargs $c_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]

	set c2_logtype [lindex $logset 2]
	set c2_logargs [adjust_logargs $c2_logtype]
	set c2_txnargs [adjust_txnargs $c2_logtype]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
	    -home $masterdir $verbargs -errpfx MASTER \
	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]

	# Open two clients
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
	    -home $clientdir $verbargs -errpfx CLIENT1 \
	    -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]

	repladd 3
	set cl2_envcmd "berkdb_env_noerr -create $c2_txnargs $c2_logargs \
	    -home $clientdir2 $verbargs -errpfx CLIENT2 \
	    -rep_transport \[list 3 replsend\]"
	set cl2env [eval $cl2_envcmd $recargs -rep_client]

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2} {$cl2env 3}"
	process_msgs $envlist

	# Run rep_test in the master (and update clients).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
	process_msgs $envlist

	# Databases should now have identical contents.
	set dbname "test.db"
	if { [is_hash $method] == 0 } {
		set db1 [berkdb_open_noerr -env $masterenv -auto_commit $dbname]
		set db2 [berkdb_open_noerr -env $clientenv -auto_commit $dbname]
		set db3 [berkdb_open_noerr -env $cl2env -auto_commit $dbname]

		error_check_good compare1and2 [db_compare \
		    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
		error_check_good compare1and3 [db_compare \
		    $db1 $db3 $masterdir/$dbname $clientdir2/$dbname] 0
		error_check_good db1_close [$db1 close] 0
		error_check_good db2_close [$db2 close] 0
		error_check_good db3_close [$db3 close] 0
	}

	puts "\tRep$tnum.b: Close client 1 and make master changes."
	error_check_good client_close [$clientenv close] 0

	# Change master and propagate changes to client 2.
	set start $niter
	eval rep_test $method $masterenv NULL $niter $start $start 0 0 $largs
	set envlist "{$masterenv 1} {$cl2env 3}"
	process_msgs $envlist

	# We need to do a deletion here to cause meta-page updates,
	# particularly for queue.  Delete the first pair and remember
	# what it is -- it should come back after the master is closed
	# and reopened as a client.
	set db1 [berkdb_open_noerr -env $masterenv -auto_commit $dbname]
	error_check_good dbopen [is_valid_db $db1] TRUE
	set txn [$masterenv txn]
	set c [$db1 cursor -txn $txn]
	error_check_good db_cursor [is_valid_cursor $c $db1] TRUE
	set first [$c get -first]
	set pair [lindex [$c get -first] 0]
	set key [lindex $pair 0]
	set data [lindex $pair 1]
	error_check_bad dbcget [llength $key] 0
	error_check_good cursor_del [$c del] 0
	error_check_good dbcclose [$c close] 0
	error_check_good txn_commit [$txn commit] 0
	error_check_good db1_close [$db1 close] 0
	#
	# Process the messages to get them out of the db.  This also
	# propagates the delete to client 2.
	#
	process_msgs $envlist

	# Nuke those for closed client
	replclear 2

	# Databases 1 and 3 should now have identical contents.
	# Database 2 should be different.  First check 1 and 3.  We
	# have to wait to check 2 until the env is open again.
	set db1 [berkdb_open_noerr -env $masterenv -auto_commit $dbname]
	set db3 [berkdb_open_noerr -env $cl2env -auto_commit $dbname]

	error_check_good compare1and3 \
	    [db_compare $db1 $db3 $masterdir/$dbname $clientdir2/$dbname] 0
	error_check_good db1_close [$db1 close] 0

	puts "\tRep$tnum.c: Close master, reopen client as master."
	error_check_good master_close [$masterenv close] 0

	set newmasterenv [eval $cl_envcmd $recargs -rep_master]
	# Now we can check that database 2 does not match 3.
	if { [is_hash $method] == 0 } {
		set db2 \
		    [berkdb_open_noerr -env $newmasterenv -auto_commit $dbname]

		error_check_good db_compare [db_compare $db2 $db3 \
		    $clientdir/$dbname $clientdir2/$dbname] 1
		error_check_good db2_close [$db2 close] 0
	}
	error_check_good db3_close [$db3 close] 0

	puts "\tRep$tnum.d: Make incompatible changes to new master."
	set envlist "{$newmasterenv 2} {$cl2env 3}"
	process_msgs $envlist

	set db [berkdb_open_noerr \
	    -env $newmasterenv -auto_commit -create $omethod test.db]
	error_check_good dbopen [is_valid_db $db] TRUE
	set t [$newmasterenv txn]
	# Force in a pair {10 10}.  This works for all access
	# methods and won't overwrite the old first pair for record-based.
	set ret [$db put -txn $t 10 [chop_data $method 10]]
	error_check_good put $ret 0
	error_check_good txn [$t commit] 0
	error_check_good dbclose [$db close] 0

	eval rep_test $method $newmasterenv NULL $niter $start $start 0 0 $largs
	set cl2rec 0
	set envlist "{$newmasterenv 2} {$cl2env 3}"
	process_msgs $envlist

	# Nuke those for closed old master
	replclear 1

	# Databases 2 and 3 should now match.
	set db2 [berkdb_open_noerr -env $newmasterenv -auto_commit $dbname]
	set db3 [berkdb_open_noerr -env $cl2env -auto_commit $dbname]

	error_check_good compare2and3 \
	    [db_compare $db2 $db3 $clientdir/$dbname $clientdir2/$dbname] 0
	error_check_good db2_close [$db2 close] 0
	error_check_good db3_close [$db3 close] 0

	puts "\tRep$tnum.e: Open old master as client."
	set newclientenv [eval $ma_envcmd -rep_client -recover]
	set envlist "{$newclientenv 1} {$newmasterenv 2} {$cl2env 3}"
	process_msgs $envlist

	# The pair we deleted earlier from the master should now
	# have reappeared.
	set db1 [berkdb_open_noerr -env $newclientenv -auto_commit $dbname]
	error_check_good dbopen [is_valid_db $db1] TRUE
	set ret [$db1 get -get_both $key [pad_data $method $data]]
	error_check_good get_both $ret [list $pair]
	error_check_good db1_close [$db1 close] 0

	set start [expr $niter * 2]
	eval rep_test $method $newmasterenv NULL $niter $start $start 0 0 $largs
	set envlist "{$newclientenv 1} {$newmasterenv 2} {$cl2env 3}"
	process_msgs $envlist

	# Now all 3 should match again.
	set db1 [berkdb_open_noerr -env $newclientenv -auto_commit $dbname]
	set db2 [berkdb_open_noerr -env $newmasterenv -auto_commit $dbname]
	set db3 [berkdb_open_noerr -env $cl2env -auto_commit $dbname]

	error_check_good compare1and2 \
	    [db_compare $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	error_check_good compare1and3 \
	    [db_compare $db1 $db3 $masterdir/$dbname $clientdir2/$dbname] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0
	error_check_good db3_close [$db3 close] 0

	error_check_good newmasterenv_close [$newmasterenv close] 0
	error_check_good newclientenv_close [$newclientenv close] 0
	error_check_good cl2_close [$cl2env close] 0
	replclose $testdir/MSGQUEUEDIR
	set testdir $orig_tdir
	return
}
