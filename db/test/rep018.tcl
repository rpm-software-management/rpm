# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep018.tcl,v 1.8 2004/09/22 18:01:06 bostic Exp $
#
# TEST	rep018
# TEST	Replication with dbremove.
# TEST
# TEST	Verify that the attempt to remove a database file
# TEST	on the master hangs while another process holds a
# TEST	handle on the client.
# TEST
proc rep018 { method { niter 10 } { tnum "018" } args } {
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
			puts "Rep$tnum ($method $r): Replication with dbremove."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep018_sub $method $niter $tnum $l $r $args
		}
	}
}

proc rep018_sub { method niter tnum logset recargs largs } {
	source ./include.tcl
	env_cleanup $testdir
	set omethod [convert_method $method]

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

	puts "\tRep$tnum.a: Create master and client, bring online."
	# Open a master.
	repladd 1
	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 -home $masterdir \
	    $m_txnargs $m_logargs -rep_master \
	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M) $recargs]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set env_cmd(C) "berkdb_env_noerr -create -home $clientdir \
	    $c_txnargs $c_logargs -rep_client \
	    -rep_transport \[list 2 replsend\]"
	set clientenv [eval $env_cmd(C) $recargs]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the client online.
	process_msgs "{$masterenv 1} {$clientenv 2}"

	puts "\tRep$tnum.b: Open database on master, propagate to client."
	set dbname rep$tnum.db
	set db [eval "berkdb_open -create $omethod -auto_commit \
	    -env $masterenv $largs $dbname"]
	set t [$masterenv txn]
	for { set i 1 } { $i <= $niter } { incr i } {
		error_check_good db_put \
		    [eval $db put -txn $t $i [chop_data $method data$i]] 0
	}
	error_check_good txn_commit [$t commit] 0
	process_msgs "{$masterenv 1} {$clientenv 2}"

	puts "\tRep$tnum.c: Spawn a child tclsh to do client work."
	set pid [exec $tclsh_path $test_path/wrap.tcl \
	    rep018script.tcl $testdir/rep018script.log \
		   $clientdir $niter $dbname $method &]

	puts "\tRep$tnum.d: Close and remove database on master."
	error_check_good close_master_db [$db close] 0

	# Remove database in master env.  First make sure the child
	# tclsh is done reading the data.
	while { 1 } {
		if { [file exists $testdir/marker.db] == 0  } {
			tclsleep 1
		} else {
			set markerenv [berkdb_env -home $testdir -txn]
			error_check_good markerenv_open \
			    [is_valid_env $markerenv] TRUE
			set marker [berkdb_open -unknown -env $markerenv \
			    -auto_commit marker.db]
			while { [llength [$marker get CHILDREADY]] == 0 } {
				tclsleep 1
			}
			break
		}
	}
	error_check_good db_remove [$masterenv dbremove -auto_commit $dbname] 0

	puts "\tRep$tnum.e: Create new database on master with the same name."
	set db [eval "berkdb_open -create $omethod -auto_commit \
	    -env $masterenv $largs $dbname"]
	error_check_good new_db_open [is_valid_db $db] TRUE

	puts "\tRep$tnum.f: Propagate changes to client.  Process should hang."
	error_check_good timestamp_remove \
	    [$marker put -auto_commit PARENTREMOVE [timestamp -r]] 0
	process_msgs "{$masterenv 1} {$clientenv 2}"
	error_check_good timestamp_done \
	    [$marker put -auto_commit PARENTDONE [timestamp -r]] 0

	watch_procs $pid 5

	puts "\tRep$tnum.g: Check for failure."
	# Check marker file for correct timestamp ordering.
	set ret [$marker get CHILDDONE]
	set childdone [lindex [lindex [lindex $ret 0] 1] 0]
	set ret [$marker get PARENTDONE]
	set parentdone [lindex [lindex [lindex $ret 0] 1] 0]
	if { [expr $childdone - $parentdone] > 0 } {
		puts "\tFAIL: parent must complete after child"
	}

	# Clean up.
	error_check_good marker_db_close [$marker close] 0
	error_check_good market_env_close [$markerenv close] 0
	error_check_good masterdb_close [$db close] 0
	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0

	replclose $testdir/MSGQUEUEDIR

	# Check log file for failures.
	set errstrings [eval findfail $testdir/rep018script.log]
	foreach str $errstrings {
		puts "FAIL: error message in rep018 log file: $str"
	}
}


