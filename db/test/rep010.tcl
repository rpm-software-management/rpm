# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep010.tcl,v 11.10 2004/09/22 18:01:06 bostic Exp $
#
# TEST  rep010
# TEST	Replication and ISPERM
# TEST
# TEST	With consecutive message processing, make sure every
# TEST	DB_REP_PERMANENT is responded to with an ISPERM when
# TEST	processed.  With gaps in the processing, make sure
# TEST	every DB_REP_PERMANENT is responded to with an ISPERM
# TEST	or a NOTPERM.  Verify in both cases that the LSN returned
# TEST	with ISPERM is found in the log.
proc rep010 { method { niter 100 } { tnum "010" } args } {

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
			puts "Rep$tnum ($method $r): Replication and ISPERM."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep010_sub $method $niter $tnum $l $r $args
		}
	}
}

proc rep010_sub { method niter tnum logset recargs largs } {
	source ./include.tcl
	global rand_init
	berkdb srand $rand_init
	global perm_sent_list

	env_cleanup $testdir
	set omethod [convert_method $method]

	replsetup $testdir/MSGQUEUEDIR
	set perm_sent_list {{}}

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
	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 $m_logargs \
	    -home $masterdir $m_txnargs -rep_master \
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
	rep010_process_msgs $masterenv $clientenv 1

	# Open database in master, propagate to client.
	set dbname rep010.db
	set db1 [eval {berkdb_open -create} $omethod -auto_commit \
	    -env $masterenv $largs $dbname]
	rep010_process_msgs $masterenv $clientenv 1

	puts "\tRep$tnum.a: Process messages with no gaps."
	# Feed operations one at a time to master and immediately
	# update client.
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		error_check_good db_put \
		    [eval $db1 put -txn $t $i [chop_data $method data$i]] 0
		error_check_good txn_commit [$t commit] 0
		rep010_process_msgs $masterenv $clientenv 1
	}

	# Replace data.
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		set ret \
		    [$db1 get -get_both -txn $t $i [pad_data $method data$i]]
		error_check_good db_put \
		    [$db1 put -txn $t $i [chop_data $method newdata$i]] 0
		error_check_good txn_commit [$t commit] 0
		rep010_process_msgs $masterenv $clientenv 1
	}

	# Try some aborts.  These do not write permanent messages.
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		error_check_good db_put [$db1 put -txn $t $i abort$i] 0
		error_check_good txn_abort [$t abort] 0
		rep010_process_msgs $masterenv $clientenv 0
	}

	puts "\tRep$tnum.b: Process messages with gaps."
	# To test gaps in message processing, run and commit a whole
	# bunch of transactions, then process the messages with skips.
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		error_check_good db_put [$db1 put -txn $t $i data$i] 0
		error_check_good txn_commit [$t commit] 0
	}
	set skip [berkdb random_int 2 8]
	rep010_process_msgs $masterenv $clientenv 1 $skip

	# Clean up.
	error_check_good db1_close [$db1 close] 0
	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0

	replclose $testdir/MSGQUEUEDIR
}

proc rep010_process_msgs { masterenv clientenv check {skip_interval 0} } {
	global perm_response_list
	global perm_sent_list

	set perm_response_list {{}}

	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1 $skip_interval]
		incr nproced [replprocessqueue $clientenv 2 $skip_interval]

		# In this test, the ISPERM and NOTPERM messages are
		# sent by the client back to the master.  Verify that we
		# get ISPERM when the client is caught up to the master
		# (i.e. last client LSN in the log matches the LSN returned
		# with the ISPERM), and that when we get NOTPERM, the client
		# is not caught up.

		# Create a list of the LSNs in the client log.
		set lsnlist {}
		set logc [$clientenv log_cursor]
		error_check_good logc \
		    [is_valid_logc $logc $clientenv] TRUE
		for { set logrec [$logc get -first] } \
		    { [llength $logrec] != 0 } \
		    { set logrec [$logc get -next] } {
			lappend lsnlist [lindex [lindex $logrec 0] 1]
		}
		set lastloglsn [lindex $lsnlist end]

		# Parse perm_response_list to find the LSN returned with
		# ISPERM or NOTPERM.
		set response [lindex $perm_response_list end]
		set permtype [lindex $response 0]
		set messagelsn [lindex [lindex $response 1] 1]

		if { [llength $response] != 0 } {
			if { $permtype == "NOTPERM" } {
				# If we got a NOTPERM, the returned LSN has to
				# be greater than the last LSN in the log.
				error_check_good notpermlsn \
				    [expr $messagelsn > $lastloglsn] 1
			} elseif { $permtype == "ISPERM" } {
				# If we got an ISPERM, the returned LSN has to
				# be in the log.
				error_check_bad \
				    ispermlsn [lsearch $lsnlist $messagelsn] -1
			} else {
				puts "FAIL: unexpected message type $permtype"
			}
		}

		error_check_good logc_close [$logc close] 0

		# If we've finished processing all the messages, check
		# that the last received permanent message LSN matches the
		# last sent permanent message LSN.
		if { $nproced == 0 } {
			if { $check != 0 } {
				set last_sent [lindex $perm_sent_list end]
				set last_rec_msg \
				    [lindex $perm_response_list end]
				set last_received [lindex $last_rec_msg 1]
				error_check_good last_message \
				    $last_sent $last_received
			}

			# If we check correctly; empty out the lists
			set perm_response_list {{}}
			set perm_sent_list {{}}
			break
		}
	}
}
