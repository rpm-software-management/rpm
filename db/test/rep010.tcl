# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep010.tcl,v 11.3 2003/10/31 20:15:43 sandstro Exp $
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
	source ./include.tcl
	global perm_sent_list
	global perm_rec_list
	global rand_init
	berkdb srand $rand_init
	puts "Rep$tnum: Replication and ISPERM ($method)"

	env_cleanup $testdir
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	# Open a master.  
	repladd 1
	set env_cmd(M) "berkdb_env_noerr -create -lock_max 2500 \
	    -log_max 1000000 \
	    -home $masterdir -txn nosync -rep_master \
	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M)]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set env_cmd(C) "berkdb_env_noerr -create -home $clientdir \
	    -txn nosync -rep_client \
	    -rep_transport \[list 2 replsend\]"
	set clientenv [eval $env_cmd(C)]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the client online.
	rep010_procmessages $masterenv $clientenv

	# Open database in master, propagate to client.
	set dbname rep010.db
	set db1 [eval "berkdb_open -create $omethod -auto_commit \
	    -env $masterenv $args $dbname"]
	rep010_procmessages $masterenv $clientenv

	puts "\tRep$tnum.a: Process messages with no gaps."
	# Initialize lists of permanent LSNs sent and received.
	set perm_sent_list {}
	set perm_rec_list {}

	# Feed operations one at a time to master and immediately 
	# update client.  
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		error_check_good db_put \
		    [eval $db1 put -txn $t $i [chop_data $method data$i]] 0
		error_check_good txn_commit [$t commit] 0
		rep010_procmessages $masterenv $clientenv
	}

	# Replace data. 
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		set ret \
		    [$db1 get -get_both -txn $t $i [pad_data $method data$i]]
		error_check_good db_put \
		    [$db1 put -txn $t $i [chop_data $method newdata$i]] 0
		error_check_good txn_commit [$t commit] 0
		rep010_procmessages $masterenv $clientenv
	}

	# Try some aborts.  These do not write permanent messages. 
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		error_check_good db_put [$db1 put -txn $t $i abort$i] 0
		error_check_good txn_abort [$t abort] 0
		rep010_procmessages $masterenv $clientenv
	}

	puts "\tRep$tnum.b: Process messages with gaps."
	# Reinitialize lists of permanent LSNs sent and received.
	set perm_sent_list {}
	set perm_rec_list {}

	# To test gaps in message processing, run and commit a whole 
	# bunch of transactions, then process the messages with skips.
	for { set i 1 } { $i <= $niter } { incr i } {
		set t [$masterenv txn]
		error_check_good db_put [$db1 put -txn $t $i data$i] 0
		error_check_good txn_commit [$t commit] 0
	}
	set skip [berkdb random_int 2 8]
	rep010_procmessages $masterenv $clientenv $skip

	# Clean up.
	error_check_good db1_close [$db1 close] 0
	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0

	replclose $testdir/MSGQUEUEDIR
}

proc rep010_procmessages { masterenv clientenv {skip_interval 0} } {
	global perm_response
	global perm_sent_list
	global perm_rec_list

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
		for { set logrec [$logc get -first] }  { [llength $logrec] != 0 } \
		    { set logrec [$logc get -next] } {
			lappend lsnlist [lindex [lindex $logrec 0] 1]
		} 
		set lastloglsn [lindex $lsnlist end]

		# Parse perm_response to find the LSN returned with 
		# ISPERM or NOTPERM.
		set permtype [lindex $perm_response 0]
		set messagelsn [lindex [lindex $perm_response 1] 1]

		if { $perm_response != "" } {
			if { $permtype == "NOTPERM" } {
				# If we got a NOTPERM, the returned LSN has to 
				# be greater than the last LSN in the log.
				error_check_good \
				    notpermlsn [expr $messagelsn > $lastloglsn] 1
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
			set last_sent [string index $perm_sent_list end]
			set last_received [string index $perm_rec_list end]
			error_check_good last_message $last_sent $last_received
			break
		}
	}
}

