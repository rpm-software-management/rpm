# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: reputils.tcl,v 11.58 2003/10/31 20:15:43 sandstro Exp $
#
# Replication testing utilities

# Environment handle for the env containing the replication "communications
# structure" (really a CDB environment).

# The test environment consists of a queue and a # directory (environment)
# per replication site.  The queue is used to hold messages destined for a
# particular site and the directory will contain the environment for the
# site.  So the environment looks like:
#				$testdir
#			 ___________|______________________________
#			/           |              \               \
#		MSGQUEUEDIR	MASTERDIR	CLIENTDIR.0 ...	CLIENTDIR.N-1
#		| | ... |
#		1 2 .. N+1
#
# The master is site 1 in the MSGQUEUEDIR and clients 1-N map to message
# queues 2 - N+1.
#
# The globals repenv(1-N) contain the environment handles for the sites
# with a given id (i.e., repenv(1) is the master's environment.

global queueenv

# Array of DB handles, one per machine ID, for the databases that contain
# messages.
global queuedbs
global machids
global perm_sent_list
set perm_sent_list {}
global perm_rec_list
set perm_rec_list {}
global elect_timeout
set elect_timeout 50000000
set drop 0

# Create the directory structure for replication testing.
# Open the master and client environments; store these in the global repenv
# Return the master's environment: "-env masterenv"
proc repl_envsetup { envargs largs test {nclients 1} {droppct 0} { oob 0 } } {
	source ./include.tcl
	global clientdir
	global drop drop_msg
	global masterdir
	global repenv
	global testdir

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir
	if { $droppct != 0 } {
		set drop 1
		set drop_msg [expr 100 / $droppct]
	} else {
		set drop 0
	}

	for { set i 0 } { $i < $nclients } { incr i } {
		set clientdir($i) $testdir/CLIENTDIR.$i
		file mkdir $clientdir($i)
	}

	# Open a master.
	repladd 1
	#
	# Set log smaller than default to force changing files,
	# but big enough so that the tests that use binary files
	# as keys/data can run.
	#
	set logmax [expr 3 * 1024 * 1024]
	set masterenv [eval {berkdb_env -create -log_max $logmax} $envargs \
	    -lock_max 10000 \
	    {-home $masterdir -txn nosync -rep_master -rep_transport \
	    [list 1 replsend]}]
	error_check_good master_env [is_valid_env $masterenv] TRUE
	set repenv(master) $masterenv

	# Open clients
	for { set i 0 } { $i < $nclients } { incr i } {
		set envid [expr $i + 2]
		repladd $envid
                set clientenv [eval {berkdb_env -create} $envargs -txn nosync \
		    {-cachesize { 0 10000000 0 }} -lock_max 10000 \
		    { -home $clientdir($i) -rep_client -rep_transport \
		    [list $envid replsend]}]
		error_check_good client_env [is_valid_env $clientenv] TRUE
		set repenv($i) $clientenv
	}
	set repenv($i) NULL
	append largs " -env $masterenv "

	# Process startup messages
	repl_envprocq $test $nclients $oob

	return $largs
}

# Process all incoming messages.  Iterate until there are no messages left
# in anyone's queue so that we capture all message exchanges. We verify that
# the requested number of clients matches the number of client environments
# we have.  The oob parameter indicates if we should process the queue
# with out-of-order delivery.  The replprocess procedure actually does
# the real work of processing the queue -- this routine simply iterates
# over the various queues and does the initial setup.
proc repl_envprocq { test { nclients 1 } { oob 0 }} {
	global repenv
	global drop

	set masterenv $repenv(master)
	for { set i 0 } { 1 } { incr i } {
		if { $repenv($i) == "NULL"} {
			break
		}
	}
	error_check_good i_nclients $nclients $i

	berkdb debug_check
	puts -nonewline "\t$test: Processing master/$i client queues"
	set rand_skip 0
	if { $oob } {
		puts " out-of-order"
	} else {
		puts " in order"
	}
	set do_check 1
	set droprestore $drop
	while { 1 } {
		set nproced 0

		if { $oob } {
			set rand_skip [berkdb random_int 2 10]
		}
		incr nproced [replprocessqueue $masterenv 1 $rand_skip]
		for { set i 0 } { $i < $nclients } { incr i } {
			set envid [expr $i + 2]
			if { $oob } {
				set rand_skip [berkdb random_int 2 10]
			}
			set n [replprocessqueue $repenv($i) \
			    $envid $rand_skip]
			incr nproced $n
		}

		if { $nproced == 0 } {
			# Now that we delay requesting records until
			# we've had a few records go by, we should always
			# see that the number of requests is lower than the
			# number of messages that were enqueued.
			for { set i 0 } { $i < $nclients } { incr i } {
				set clientenv $repenv($i)
				set stats [$clientenv rep_stat]
				set queued [getstats $stats  \
				   {Total log records queued}]
				error_check_bad queued_stats \
				    $queued -1
				set requested [getstats $stats \
				   {Log records requested}]
				error_check_bad requested_stats \
				    $requested -1
				if { $queued != 0 && $do_check != 0 } {
					error_check_good num_requested \
					    [expr $requested < $queued] 1
				}

				$clientenv rep_request 1 1
			}

			# If we were dropping messages, we might need
			# to flush the log so that we get everything
			# and end up in the right state.
			if { $drop != 0 } {
				set drop 0
				set do_check 0
				$masterenv rep_flush
				berkdb debug_check
				puts "\t$test: Flushing Master"
			} else {
				break
			}
		}
	}

	# Reset the clients back to the default state in case we
	# have more processing to do.
	for { set i 0 } { $i < $nclients } { incr i } {
		set clientenv $repenv($i)
		$clientenv rep_request 4 128
	}
	set drop $droprestore
}

# Verify that the directories in the master are exactly replicated in
# each of the client environments.
proc repl_envver0 { test method { nclients 1 } } {
	global clientdir
	global masterdir
	global repenv

	# Verify the database in the client dir.
	# First dump the master.
	set t1 $masterdir/t1
	set t2 $masterdir/t2
	set t3 $masterdir/t3
	set omethod [convert_method $method]

	#
	# We are interested in the keys of whatever databases are present
	# in the master environment, so we just call a no-op check function
	# since we have no idea what the contents of this database really is.
	# We just need to walk the master and the clients and make sure they
	# have the same contents.
	#
	set cwd [pwd]
	cd $masterdir
	set stat [catch {glob test*.db} dbs]
	cd $cwd
	if { $stat == 1 } {
		return
	}
	foreach testfile $dbs {
		open_and_dump_file $testfile $repenv(master) $masterdir/t2 \
		    repl_noop dump_file_direction "-first" "-next"

		if { [string compare [convert_method $method] -recno] != 0 } {
			filesort $t2 $t3
			file rename -force $t3 $t2
		}
		for { set i 0 } { $i < $nclients } { incr i } {
	puts "\t$test: Verifying client $i database $testfile contents."
			open_and_dump_file $testfile $repenv($i) \
			    $t1 repl_noop dump_file_direction "-first" "-next"

			if { [string compare $omethod "-recno"] != 0 } {
				filesort $t1 $t3
			} else {
				catch {file copy -force $t1 $t3} ret
			}
			error_check_good diff_files($t2,$t3) [filecmp $t2 $t3] 0
		}
	}
}

# Remove all the elements from the master and verify that these
# deletions properly propagated to the clients.
proc repl_verdel { test method { nclients 1 } } {
	global clientdir
	global masterdir
	global repenv

	# Delete all items in the master.
	set cwd [pwd]
	cd $masterdir
	set stat [catch {glob test*.db} dbs]
	cd $cwd
	if { $stat == 1 } {
		return
	}
	foreach testfile $dbs {
		puts "\t$test: Deleting all items from the master."
		set txn [$repenv(master) txn]
		error_check_good txn_begin [is_valid_txn $txn \
		    $repenv(master)] TRUE
		set db [berkdb_open -txn $txn -env $repenv(master) $testfile]
		error_check_good reopen_master [is_valid_db $db] TRUE
		set dbc [$db cursor -txn $txn]
		error_check_good reopen_master_cursor \
		    [is_valid_cursor $dbc $db] TRUE
		for { set dbt [$dbc get -first] } { [llength $dbt] > 0 } \
		    { set dbt [$dbc get -next] } {
			error_check_good del_item [$dbc del] 0
		}
		error_check_good dbc_close [$dbc close] 0
		error_check_good txn_commit [$txn commit] 0
		error_check_good db_close [$db close] 0

		repl_envprocq $test $nclients

		# Check clients.
		for { set i 0 } { $i < $nclients } { incr i } {
			puts "\t$test: Verifying client database $i is empty."

			set db [berkdb_open -env $repenv($i) $testfile]
			error_check_good reopen_client($i) \
			    [is_valid_db $db] TRUE
			set dbc [$db cursor]
			error_check_good reopen_client_cursor($i) \
			    [is_valid_cursor $dbc $db] TRUE

			error_check_good client($i)_empty \
			    [llength [$dbc get -first]] 0

			error_check_good dbc_close [$dbc close] 0
			error_check_good db_close [$db close] 0
		}
	}
}

# Replication "check" function for the dump procs that expect to
# be able to verify the keys and data.
proc repl_noop { k d } {
	return
}

# Close all the master and client environments in a replication test directory.
proc repl_envclose { test envargs } {
	source ./include.tcl
	global clientdir
	global encrypt
	global masterdir
	global repenv
	global testdir

	if { [lsearch $envargs "-encrypta*"] !=-1 } {
		set encrypt 1
	}

	# In order to make sure that we have fully-synced and ready-to-verify
	# databases on all the clients, do a checkpoint on the master and
	# process messages in order to flush all the clients.
	set drop 0
	set do_check 0
	berkdb debug_check
	puts "\t$test: Checkpointing master."
	error_check_good masterenv_ckp [$repenv(master) txn_checkpoint] 0

	# Count clients.
	for { set ncli 0 } { 1 } { incr ncli } {
		if { $repenv($ncli) == "NULL" } {
			break
		}
	}
	repl_envprocq $test $ncli

	error_check_good masterenv_close [$repenv(master) close] 0
	verify_dir $masterdir "\t$test: " 0 0 1
	for { set i 0 } { $i < $ncli } { incr i } {
		error_check_good client($i)_close [$repenv($i) close] 0
		verify_dir $clientdir($i) "\t$test: " 0 0 1
	}
	replclose $testdir/MSGQUEUEDIR

}

# Close up a replication group
proc replclose { queuedir } {
	global queueenv queuedbs machids

	foreach m $machids {
		set db $queuedbs($m)
		error_check_good dbr_close [$db close] 0
	}
	error_check_good qenv_close [$queueenv close] 0
	set machids {}
}

# Create a replication group for testing.
proc replsetup { queuedir } {
	global queueenv queuedbs machids

	file mkdir $queuedir
	set queueenv \
	    [berkdb_env -create -txn nosync -lock_max 20000 -home $queuedir]
	error_check_good queueenv [is_valid_env $queueenv] TRUE

	if { [info exists queuedbs] } {
		unset queuedbs
	}
	set machids {}

	return $queueenv
}

# Send function for replication.
proc replsend { control rec fromid toid flags lsn } {
	global queuedbs queueenv machids
	global drop drop_msg
	global perm_sent_list

	if { $flags == "perm" } {
#		puts "replsend sent perm message, LSN $lsn"
		lappend perm_sent_list $lsn
	}

	#
	# If we are testing with dropped messages, then we drop every
	# $drop_msg time.  If we do that just return 0 and don't do
	# anything.
	#
	if { $drop != 0 } {
		incr drop
		if { $drop == $drop_msg } {
			set drop 1
			return 0
		}
	}
	# XXX
	# -1 is DB_BROADCAST_MID
	if { $toid == -1 } {
		set machlist $machids
	} else {
		if { [info exists queuedbs($toid)] != 1 } {
			error "replsend: machid $toid not found"
		}
		set machlist [list $toid]
	}

	foreach m $machlist {
		# XXX should a broadcast include to "self"?
		if { $m == $fromid } {
			continue
		}

		set db $queuedbs($m)
		set txn [$queueenv txn]
		$db put -txn $txn -append [list $control $rec $fromid]
		error_check_good replsend_commit [$txn commit] 0
	}

	return 0
}

# Nuke all the pending messages for a particular site.
proc replclear { machid } {
	global queuedbs queueenv

	if { [info exists queuedbs($machid)] != 1 } {
		error "FAIL: replclear: machid $machid not found"
	}

	set db $queuedbs($machid)
	set txn [$queueenv txn]
	set dbc [$db cursor -txn $txn]
	for { set dbt [$dbc get -rmw -first] } { [llength $dbt] > 0 } \
	    { set dbt [$dbc get -rmw -next] } {
		error_check_good replclear($machid)_del [$dbc del] 0
	}
	error_check_good replclear($machid)_dbc_close [$dbc close] 0
	error_check_good replclear($machid)_txn_commit [$txn commit] 0
}

# Add a machine to a replication environment.
proc repladd { machid } {
	global queueenv queuedbs machids

	if { [info exists queuedbs($machid)] == 1 } {
		error "FAIL: repladd: machid $machid already exists"
	}

	set queuedbs($machid) [berkdb open -auto_commit \
	    -env $queueenv -create -recno -renumber repqueue$machid.db]
	error_check_good repqueue_create [is_valid_db $queuedbs($machid)] TRUE

	lappend machids $machid
}

# Process a queue of messages, skipping every "skip_interval" entry.
# We traverse the entire queue, but since we skip some messages, we
# may end up leaving things in the queue, which should get picked up
# on a later run.
proc replprocessqueue { dbenv machid { skip_interval 0 } { hold_electp NONE } \
    { newmasterp NONE } { dupmasterp NONE } { errp NONE } } {
	global queuedbs queueenv errorCode
	global perm_response
	set perm_response ""
	global perm_rec_list

	# hold_electp is a call-by-reference variable which lets our caller
	# know we need to hold an election.
	if { [string compare $hold_electp NONE] != 0 } {
		upvar $hold_electp hold_elect
	}
	set hold_elect 0

	# newmasterp is the same idea, only returning the ID of a master
	# given in a DB_REP_NEWMASTER return.
	if { [string compare $newmasterp NONE] != 0 } {
		upvar $newmasterp newmaster
	}
	set newmaster 0

	# dupmasterp is a call-by-reference variable which lets our caller
	# know we have a duplicate master.
	if { [string compare $dupmasterp NONE] != 0 } {
		upvar $dupmasterp dupmaster
	}
	set dupmaster 0

	# errp is a call-by-reference variable which lets our caller
	# know we have gotten an error (that they expect).
	if { [string compare $errp NONE] != 0 } {
		upvar $errp errorp
	}
	set errorp 0

	set nproced 0

	set txn [$queueenv txn]
	set dbc [$queuedbs($machid) cursor -txn $txn]

	error_check_good process_dbc($machid) \
	    [is_valid_cursor $dbc $queuedbs($machid)] TRUE

	for { set dbt [$dbc get -first] } \
	    { [llength $dbt] != 0 } \
	    { set dbt [$dbc get -next] } {
		set data [lindex [lindex $dbt 0] 1]

		# If skip_interval is nonzero, we want to process messages
		# out of order.  We do this in a simple but slimy way--
		# continue walking with the cursor without processing the
		# message or deleting it from the queue, but do increment
		# "nproced".  The way this proc is normally used, the
		# precise value of nproced doesn't matter--we just don't
		# assume the queues are empty if it's nonzero.  Thus,
		# if we contrive to make sure it's nonzero, we'll always
		# come back to records we've skipped on a later call
		# to replprocessqueue.  (If there really are no records,
		# we'll never get here.)
		#
		# Skip every skip_interval'th record (and use a remainder other
		# than zero so that we're guaranteed to really process at least
		# one record on every call).
		if { $skip_interval != 0 } {
			if { $nproced % $skip_interval == 1 } {
				incr nproced
				continue
			}
		}

		# We have to play an ugly cursor game here:  we currently
		# hold a lock on the page of messages, but rep_process_message
		# might need to lock the page with a different cursor in
		# order to send a response.  So save our recno, close
		# the cursor, and then reopen and reset the cursor.
		set recno [lindex [lindex $dbt 0] 0]
		error_check_good dbc_process_close [$dbc close] 0
		error_check_good txn_commit [$txn commit] 0
		set ret [catch {$dbenv rep_process_message \
		    [lindex $data 2] [lindex $data 0] [lindex $data 1]} res]
		set txn [$queueenv txn]
		set dbc [$queuedbs($machid) cursor -txn $txn]
		set dbt [$dbc get -set $recno]

		# Save all ISPERM and NOTPERM responses so we can compare their
		# LSNs to the LSN in the log.  The variable perm_response holds
		# the response. 
		#
		if { [is_substr $res ISPERM] || [is_substr $res NOTPERM] } {
			set perm_response $res
			set lsn [lindex $perm_response 1]
			lappend perm_rec_list $lsn 
		}

		if { $ret != 0 } {
			if { [string compare $errp NONE] != 0 } {
				set errorp $res
			} else {
				error "FAIL:[timestamp]\
				    rep_process_message returned $res"
			}
		}

		incr nproced

		$dbc del

		if { $ret == 0 } {
			set rettype [lindex $res 0]
			set retval [lindex $res 1]
			#
			# Do nothing for 0 and NEWSITE
			#
			if { [is_substr $rettype HOLDELECTION] } {
				set hold_elect 1
			}
			if { [is_substr $rettype DUPMASTER] } {
				set dupmaster 1
			}
			if { [is_substr $rettype NOTPERM] || \
			    [is_substr $rettype ISPERM] } {
				set lsnfile [lindex $retval 0]
				set lsnoff [lindex $retval 1]
			}
			if { [is_substr $rettype NEWMASTER] } {
				set newmaster $retval
				# Break as soon as we get a NEWMASTER message;
				# our caller needs to handle it.
				break
			}
		}

		if { $errorp == 1 } {
			# Break also on an error, caller wants to handle it.
			break
		}
		if { $hold_elect == 1 } {
			# Break also on a HOLDELECTION, for the same reason.
			break
		}
		if { $dupmaster == 1 } {
			# Break also on a HOLDELECTION, for the same reason.
			break
		}

	}

	error_check_good dbc_close [$dbc close] 0
	error_check_good txn_commit [$txn commit] 0

	# Return the number of messages processed.
	return $nproced
}

set run_repl_flag "-run_repl"

proc extract_repl_args { args } {
	global run_repl_flag

	for { set arg [lindex $args [set i 0]] } \
	    { [string length $arg] > 0 } \
	    { set arg [lindex $args [incr i]] } {
		if { [string compare $arg $run_repl_flag] == 0 } {
			return [lindex $args [expr $i + 1]]
		}
	}
	return ""
}

proc delete_repl_args { args } {
	global run_repl_flag

	set ret {}

	for { set arg [lindex $args [set i 0]] } \
	    { [string length $arg] > 0 } \
	    { set arg [lindex $args [incr i]] } {
		if { [string compare $arg $run_repl_flag] != 0 } {
			lappend ret $arg
		} else {
			incr i
		}
	}
	return $ret
}

global elect_serial
global elections_in_progress
set elect_serial 0

# Start an election in a sub-process.
proc start_election { pfx qdir envstring nsites pri timeout {err "none"}} {
	source ./include.tcl
	global elect_serial elect_timeout elections_in_progress machids

	set filelist {}
	set ret [catch {glob $testdir/ELECTION*.$elect_serial} result]
	if { $ret == 0 } {
		set filelist [concat $filelist $result]
	}
	foreach f $filelist {
		fileremove -f $f
	}

	set oid [open $testdir/ELECTION_SOURCE.$elect_serial w]

	puts $oid "source $test_path/test.tcl"
	puts $oid "replsetup $qdir"
	foreach i $machids { puts $oid "repladd $i" }
	puts $oid "set env_cmd \{$envstring\}"
	puts $oid "set dbenv \[eval \$env_cmd -errfile \
	    $testdir/ELECTION_ERRFILE.$elect_serial -errpfx $pfx \]"
#	puts $oid "set dbenv \[eval \$env_cmd -errfile \
#	    /dev/stdout -errpfx $pfx \]"
	puts $oid "\$dbenv test abort $err"
	puts $oid "set res \[catch \{\$dbenv rep_elect $nsites $pri \
	    $elect_timeout\} ret\]"
	puts $oid "set r \[open \$testdir/ELECTION_RESULT.$elect_serial w\]"
	puts $oid "if \{\$res == 0 \} \{"
	puts $oid "puts \$r \"NEWMASTER \$ret\""
	puts $oid "\} else \{"
	puts $oid "puts \$r \"ERROR \$ret\""
	puts $oid "\}"
	if { $err != "none" } {
		puts $oid "\$dbenv test abort none"
		puts $oid "set res \[catch \{\$dbenv rep_elect $nsites $pri \
		    $elect_timeout\} ret\]"
		puts $oid "if \{\$res == 0 \} \{"
		puts $oid "puts \$r \"NEWMASTER \$ret\""
		puts $oid "\} else \{"
		puts $oid "puts \$r \"ERROR \$ret\""
		puts $oid "\}"
	}
	puts $oid "close \$r"
	close $oid

	set t [open "|$tclsh_path >& $testdir/ELECTION_OUTPUT.$elect_serial" w]
#	set t [open "|$tclsh_path" w]
	puts $t "source ./include.tcl"
	puts $t "source $testdir/ELECTION_SOURCE.$elect_serial"
	flush $t

	set elections_in_progress($elect_serial) $t
	return $elect_serial
}

proc check_election { id newmasterp } {
	source ./include.tcl

	if { $id == "INVALID" } {
		return 0
	}
	upvar $newmasterp newmaster
	set newmaster 0
	set res [catch {open $testdir/ELECTION_RESULT.$id} nmid]
	if { $res != 0 } {
		return 0
	}
	while { [gets $nmid val] != -1 } {
#		puts "result $id: $val"
		set str [lindex $val 0]
		if { [is_substr $str NEWMASTER] } {
			set newmaster [lindex $val 1]
		}
	}
	close $nmid
	return 1
}

proc close_election { i } {
	global elections_in_progress
	set t $elections_in_progress($i)
	puts $t "replclose \$testdir/MSGQUEUEDIR"
	puts $t "\$dbenv close"
	close $t
	unset elections_in_progress($i)
}

proc cleanup_elections { } {
	global elect_serial elections_in_progress

	for { set i 0 } { $i <= $elect_serial } { incr i } {
		if { [info exists elections_in_progress($i)] != 0 } {
			close_election $i
		}
	}

	set elect_serial 0
}

#
# This is essentially a copy of test001, but it only does the put/get
# loop AND it takes an already-opened db handle.
#
proc rep_test { method env db {nentries 10000} {start 0} {skip 1} } {
	source ./include.tcl

	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	# If we are not using an external env, then test setting
	# the database cache size and using multiple caches.
	puts "\t\tRep_test: $method $nentries key/data pairs starting at $start"
	set did [open $dict]

	# The "start" variable determines the record number to start
	# with, if we're using record numbers.  The "skip" variable
	# determines whether to start with the first entry in the
	# dict file (if skip = 0) or skip over "start" entries (skip = 1).
	# Skip is set to 1 to get different key/data pairs for
	# different iterations of replication tests.  Skip must be set
	# to 0 if we're running a test that uses 10000 iterations,
	# otherwise we run out of data to read in.

	if { $skip == 1 } {
		for { set count 0 } { $count < $start } { incr count } {
			gets $did str
		}
	}
	set pflags ""
	set gflags ""
	set txn ""

	if { [is_record_based $method] == 1 } {
		append gflags " -recno"
	}
	puts "\t\tRep_test.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			global kvals

			set key [expr $count + 1 + $start]
			if { 0xffffffff > 0 && $key > 0xffffffff } {
				set key [expr $key - 0x100000000]
			}
			if { $key == 0 || $key - 0xffffffff == 1 } {
				incr key
				incr count
			}
			set kvals($key) [pad_data $method $str]
		} else {
			set key $str
			set str [reverse $str]
		}
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0
		error_check_good txn [$t commit] 0
		if { $count % 5 == 0 } {
			error_check_good txn_checkpoint($count) \
			    [$env txn_checkpoint] 0
		}
		incr count
	}
	close $did
}
