# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: reputils.tcl,v 12.30 2006/09/13 21:51:23 carol Exp $
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


# queuedbs is an array of DB handles, one per machine ID/machine ID pair,
# for the databases that contain messages from one machine to another.
# We omit the cases where the "from" and "to" machines are the same.
# Since tcl does not have real two-dimensional arrays, we use this
# naming convention:  queuedbs(1.2) has the handle for the database
# containing messages to machid 1 from machid 2.
#
global queuedbs
global machids
global perm_response_list
set perm_response_list {}
global perm_sent_list
set perm_sent_list {}
global elect_timeout
set elect_timeout 50000000
set drop 0
global anywhere
set anywhere 0

# The default for replication testing is for logs to be on-disk.
# Mixed-mode log testing provides a mixture of on-disk and
# in-memory logging, or even all in-memory.  When testing on a
# 1-master/1-client test, we try all four options.  On a test
# with more clients, we still try four options, randomly
# selecting whether the later clients are on-disk or in-memory.
#

global mixed_mode_logging
set mixed_mode_logging 0

proc create_logsets { nsites } {
	global mixed_mode_logging
	global logsets
	global rand_init

	error_check_good set_random_seed [berkdb srand $rand_init] 0
	if { $mixed_mode_logging == 0 || $mixed_mode_logging == 2 } {
		if { $mixed_mode_logging == 0 } {
			set logmode "on-disk"
		} else {
			set logmode "in-memory"
		}
		set loglist {}
		for { set i 0 } { $i < $nsites } { incr i } {
			lappend loglist $logmode
		}
		set logsets [list $loglist]
	}
	if { $mixed_mode_logging == 1 } {
		set set1 {on-disk on-disk}
		set set2 {on-disk in-memory}
		set set3 {in-memory on-disk}
		set set4 {in-memory in-memory}

		# Start with nsites at 2 since we already set up
		# the master and first client.
		for { set i 2 } { $i < $nsites } { incr i } {
			foreach set { set1 set2 set3 set4 } {
				if { [berkdb random_int 0 1] == 0 } {
					lappend $set "on-disk"
				} else {
					lappend $set "in-memory"
				}
			}
		}
		set logsets [list $set1 $set2 $set3 $set4]
	}
	return $logsets
}

proc run_inmem { method test {display 0} {run 1} \
    {outfile stdout} {largs ""} } {
	global mixed_mode_logging
	set mixed_mode_logging 2

	set prefix [string range $test 0 2]
	if { $prefix != "rep" } {
		puts "Skipping in-mem log testing for non-rep test."
		set mixed_mode_logging 0
		return
	}

	eval run_method $method $test $display $run $outfile $largs

	# Reset to default values after run.
	set mixed_mode_logging 0
}

proc run_mixedmode { method test {display 0} {run 1} \
    {outfile stdout} {largs ""} } {
	global mixed_mode_logging
	set mixed_mode_logging 1

	set prefix [string range $test 0 2]
	if { $prefix != "rep" } {
		puts "Skipping mixed-mode log testing for non-rep test."
		set mixed_mode_logging 0
		return
	}

	eval run_method $method $test $display $run $outfile $largs

	# Reset to default values after run.
	set mixed_mode_logging 0
}

# Create the directory structure for replication testing.
# Open the master and client environments; store these in the global repenv
# Return the master's environment: "-env masterenv"
proc repl_envsetup { envargs largs test {nclients 1} {droppct 0} { oob 0 } } {
	source ./include.tcl
	global clientdir
	global drop drop_msg
	global masterdir
	global repenv
	global qtestdir testdir

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	if { ![info exists qtestdir] } {
		set qtestdir $testdir
	}

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
	set lockmax 40000
	set ma_cmd "berkdb_env -create -log_max $logmax $envargs \
	    -cachesize { 0 4194304 1 } \
	    -lock_max_objects $lockmax -lock_max_locks $lockmax \
	    -home $masterdir -txn nosync -rep_master -rep_transport \
	    \[list 1 replsend\]"
#	set ma_cmd "berkdb_env_noerr -create -log_max $logmax $envargs \
#	    -lock_max_objects $lockmax -lock_max_locks $lockmax \
#	    -verbose {rep on} -errfile /dev/stderr -errpfx $masterdir \
#	    -home $masterdir -txn nosync -rep_master -rep_transport \
#	    \[list 1 replsend\]"
	set masterenv [eval $ma_cmd]
	error_check_good master_env [is_valid_env $masterenv] TRUE
	set repenv(master) $masterenv

	# Open clients
	for { set i 0 } { $i < $nclients } { incr i } {
		set envid [expr $i + 2]
		repladd $envid
                set cl_cmd "berkdb_env -create $envargs -txn nosync \
		    -cachesize { 0 10000000 0 } \
		    -lock_max_objects $lockmax -lock_max_locks $lockmax \
		    -home $clientdir($i) -rep_client -rep_transport \
		    \[list $envid replsend\]"
#		set cl_cmd "berkdb_env_noerr -create $envargs -txn nosync \
#		    -cachesize { 0 10000000 0 } \
#		    -lock_max_objects $lockmax -lock_max_locks $lockmax \
#		    -home $clientdir($i) -rep_client -rep_transport \
#		    \[list $envid replsend\] -verbose {rep on} \
#		    -errfile /dev/stderr -errpfx $clientdir($i)"
                set clientenv [eval $cl_cmd]
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
				set queued [stat_field $clientenv rep_stat \
				   "Total log records queued"]
				error_check_bad queued_stats \
				    $queued -1
				set requested [stat_field $clientenv rep_stat \
				   "Log records requested"]
				error_check_bad requested_stats \
				    $requested -1
				if { $queued != 0 && $do_check != 0 } {
					error_check_good num_requested \
					    [expr $requested <= $queued] 1
				}

				$clientenv rep_request 1 4
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
		set db [eval berkdb_open -txn $txn -env $repenv(master) \
		    $testfile]
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

			set db [eval berkdb_open -env $repenv($i) $testfile]
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
	global qtestdir testdir

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
	replclose $qtestdir/MSGQUEUEDIR

}

# Close up a replication group - close all message dbs.
proc replclose { queuedir } {
	global queuedbs machids

	set dbs [array names queuedbs]
	foreach tofrom $dbs {
		set handle $queuedbs($tofrom)
		error_check_good db_close [$handle close] 0
		unset queuedbs($tofrom)
	}

	set machids {}
}

# Create a replication group for testing.
proc replsetup { queuedir } {
	global queuedbs machids

	file mkdir $queuedir

	# If there are any leftover handles, get rid of them.
	set dbs [array names queuedbs]
	foreach tofrom $dbs {
		unset queuedbs($tofrom)
	}
	set machids {}
}

# Replnoop is a dummy function to substitute for replsend
# when replication is off.
proc replnoop { control rec fromid toid flags lsn } {
	return 0
}

# Send function for replication.
proc replsend { control rec fromid toid flags lsn } {
	global is_repchild
	global queuedbs machids
	global drop drop_msg
	global perm_sent_list
	global anywhere
	global qtestdir testdir

	if { ![info exists qtestdir] } {
		set qtestdir $testdir
	}
	set queuedir $qtestdir/MSGQUEUEDIR
	set permflags [lsearch $flags "perm"]
	if { [llength $perm_sent_list] != 0 && $permflags != -1 } {
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
	# -1 is DB_BROADCAST_EID
	if { $toid == -1 } {
		set machlist $machids
	} else {
		set m NULL
		# If we can send this anywhere, send it to the first id
		# we find that is neither toid or fromid.  If we don't
		# find any other candidates, this falls back to the
		# original toid.
		if { $anywhere != 0 } {
			set anyflags [lsearch $flags "any"]
			if { $anyflags != -1 } {
				foreach m $machids {
					if { $m == $fromid || $m == $toid } {
						continue
					}
					set machlist [list $m]
					break
				}
			}
		}
		#
		# If we didn't find a different site, fall back
		# to the toid.
		#
		if { $m == "NULL" } {
			set machlist [list $toid]
		}
	}
	foreach m $machlist {
		# Do not broadcast to self.
		if { $m == $fromid } {
			continue
		}
		# Find the handle for the right message file.
		set pid [pid]
		set db $queuedbs($m.$fromid.$pid)
		set stat [catch {$db put -append [list $control $rec $fromid]} ret]
	}
	if { $is_repchild } {
		replready $fromid from
	}

	return 0
}

# Discard all the pending messages for a particular site.
proc replclear { machid {tf "to"}} {
	global queuedbs qtestdir testdir

	if { ![info exists qtestdir] } {
		set qtestdir $testdir
	}
	set queuedir $qtestdir/MSGQUEUEDIR
	set orig [pwd]

	cd $queuedir
	if { $tf == "to" } {
		set msgdbs [glob -nocomplain ready.$machid.*]
	} else {
		set msgdbs [glob -nocomplain ready.*.$machid.*]
	}
	foreach m $msgdbs {
		file delete -force $m
	}
	cd $orig
	set dbs [array names queuedbs]
	foreach tofrom $dbs {
		# Process only messages _to_ the specified machid.
		if { [string match $machid.* $tofrom] == 1 } {
			set db $queuedbs($tofrom)
			set dbc [$db cursor]
			for { set dbt [$dbc get -first] } \
			    { [llength $dbt] > 0 } \
			    { set dbt [$dbc get -next] } {
				error_check_good \
				    replclear($machid)_del [$dbc del] 0
			}
			error_check_good replclear($db)_dbc_close [$dbc close] 0
		}
	}
	cd $queuedir
	if { $tf == "to" } {
		set msgdbs [glob -nocomplain temp.$machid.*]
	} else {
		set msgdbs [glob -nocomplain temp.*.$machid.*]
	}
	foreach m $msgdbs {
#		file delete -force $m
	}
	cd $orig
}

# Makes messages available to replprocessqueue by closing and
# renaming the message files.  We ready the files for one machine
# ID at a time -- just those "to" or "from" the machine we want to
# process, depending on 'tf'.
proc replready { machid tf } {
	global queuedbs machids
	global counter
	global qtestdir testdir

	if { ![info exists qtestdir] } {
		set qtestdir $testdir
	}
	set queuedir $qtestdir/MSGQUEUEDIR

	set pid [pid]
	#
	# Close the temporary message files for the specified machine.
	# Only close it if there are messages available.
	#
	set dbs [array names queuedbs]
	set closed {}
	foreach tofrom $dbs {
		set toidx [string first . $tofrom]
		set toid [string replace $tofrom $toidx end]
		set fidx [expr $toidx + 1]
		set fromidx [string first . $tofrom $fidx]
		#
		# First chop off the end, then chop off the toid
		# in the beginning.
		#
		set fromid [string replace $tofrom $fromidx end]
		set fromid [string replace $fromid 0 $toidx]
		if { ($tf == "to" && $machid == $toid) || \
		    ($tf == "from" && $machid == $fromid) } {
			set nkeys [stat_field $queuedbs($tofrom) \
			    stat "Number of keys"]
			if { $nkeys != 0 } {
				lappend closed \
				    [list $toid $fromid temp.$tofrom]
		 		error_check_good temp_close \
				    [$queuedbs($tofrom) close] 0
			}
		}
	}

	# Rename the message files.
	set cwd [pwd]
	foreach filename $closed {
		set toid [lindex $filename 0]
		set fromid [lindex $filename 1]
		set fname [lindex $filename 2]
		set tofrom [string replace $fname 0 4]
		incr counter($machid)
		cd $queuedir
# puts "$queuedir: Msg ready $fname to ready.$tofrom.$counter($machid)"
		file rename -force $fname ready.$tofrom.$counter($machid)
		cd $cwd
		replsetuptempfile $toid $fromid $queuedir

	}
}

# Add a machine to a replication environment.  This checks
# that we have not already established that machine id, and
# adds the machid to the list of ids.
proc repladd { machid } {
	global queuedbs machids counter qtestdir testdir

	if { ![info exists qtestdir] } {
		set qtestdir $testdir
	}
	set queuedir $qtestdir/MSGQUEUEDIR
	if { [info exists machids] } {
		if { [lsearch -exact $machids $machid] >= 0 } {
			error "FAIL: repladd: machid $machid already exists."
		}
	}

	set counter($machid) 0
	lappend machids $machid

	# Create all the databases that receive messages sent _to_
	# the new machid.
	replcreatetofiles $machid $queuedir

	# Create all the databases that receive messages sent _from_
	# the new machid.
	replcreatefromfiles $machid $queuedir
}

# Creates all the databases that a machid needs for receiving messages
# from other participants in a replication group.  Used when first
# establishing the temp files, but also used whenever replready moves the
# temp files away, because we'll need new files for any future messages.
proc replcreatetofiles { toid queuedir } {
	global machids

	foreach m $machids {
		# We don't need a file for a machid to send itself messages.
		if { $m == $toid } {
			continue
		}
		replsetuptempfile $toid $m $queuedir
	}
}

# Creates all the databases that a machid needs for sending messages
# to other participants in a replication group.  Used when first
# establishing the temp files only.  Replready moves files based on
# recipient, so we recreate files based on the recipient, also.
proc replcreatefromfiles { fromid queuedir } {
	global machids

	foreach m $machids {
		# We don't need a file for a machid to send itself messages.
		if { $m == $fromid } {
			continue
		}
		replsetuptempfile $m $fromid $queuedir
	}
}

proc replsetuptempfile { to from queuedir } {
	global queuedbs

	set pid [pid]
	set queuedbs($to.$from.$pid) [berkdb open -create -excl -recno\
	    -renumber $queuedir/temp.$to.$from.$pid]
	error_check_good open_queuedbs [is_valid_db $queuedbs($to.$from.$pid)] TRUE
}

# Process a queue of messages, skipping every "skip_interval" entry.
# We traverse the entire queue, but since we skip some messages, we
# may end up leaving things in the queue, which should get picked up
# on a later run.
proc replprocessqueue { dbenv machid { skip_interval 0 } { hold_electp NONE } \
    { newmasterp NONE } { dupmasterp NONE } { errp NONE } } {
	global errorCode
	global perm_response_list
	global qtestdir testdir

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

	set queuedir $qtestdir/MSGQUEUEDIR
	replready $machid to

	# Change directories temporarily so we get just the msg file name.
	set cwd [pwd]
	cd $queuedir
	set msgdbs [glob -nocomplain ready.$machid.*]
# puts "$queuedir.$machid: My messages: $msgdbs"
	cd $cwd

	foreach msgdb $msgdbs {
		set db [berkdb_open $queuedir/$msgdb]
		set dbc [$db cursor]

		error_check_good process_dbc($machid) \
		    [is_valid_cursor $dbc $db] TRUE

		for { set dbt [$dbc get -first] } \
		    { [llength $dbt] != 0 } \
		    { set dbt [$dbc get -next] } {
			set data [lindex [lindex $dbt 0] 1]
			set recno [lindex [lindex $dbt 0] 0]

			# If skip_interval is nonzero, we want to process
			# messages out of order.  We do this in a simple but
			# slimy way -- continue walking with the cursor
			# without processing the message or deleting it from
			# the queue, but do increment "nproced".  The way
			# this proc is normally used, the precise value of
			# nproced doesn't matter--we just don't assume the
			# queues are empty if it's nonzero.  Thus, if we
			# contrive to make sure it's nonzero, we'll always
			# come back to records we've skipped on a later call
			# to replprocessqueue.  (If there really are no records,
			# we'll never get here.)
			#
			# Skip every skip_interval'th record (and use a
			# remainder other than zero so that we're guaranteed
			# to really process at least one record on every call).
			if { $skip_interval != 0 } {
				if { $nproced % $skip_interval == 1 } {
					incr nproced
					set dbt [$dbc get -next]
					continue
				}
			}

			# We need to remove the current message from the
			# queue, because we're about to end the transaction
			# and someone else processing messages might come in
			# and reprocess this message which would be bad.
			#
			error_check_good queue_remove [$dbc del] 0

			# We have to play an ugly cursor game here:  we
			# currently hold a lock on the page of messages, but
			# rep_process_message might need to lock the page with
			# a different cursor in order to send a response.  So
			# save the next recno, close the cursor, and then
			# reopen and reset the cursor.  If someone else is
			# processing this queue, our entry might have gone
			# away, and we need to be able to handle that.
			#
#			error_check_good dbc_process_close [$dbc close] 0

			set ret [catch {$dbenv rep_process_message \
			    [lindex $data 2] [lindex $data 0] \
			    [lindex $data 1]} res]

			# Save all ISPERM and NOTPERM responses so we can
			# compare their LSNs to the LSN in the log.  The
			# variable perm_response_list holds the entire
			# response so we can extract responses and LSNs as
			# needed.
			#
			if { [llength $perm_response_list] != 0 && \
			    ([is_substr $res ISPERM] || [is_substr $res NOTPERM]) } {
				lappend perm_response_list $res
			}

			if { $ret != 0 } {
				if { [string compare $errp NONE] != 0 } {
					set errorp "$dbenv $machid $res"
				} else {
					error "FAIL:[timestamp]\
					    rep_process_message returned $res"
				}
			}

			incr nproced
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
					set dupmaster "1 $dbenv $machid"
				}
				if { [is_substr $rettype NOTPERM] || \
				    [is_substr $rettype ISPERM] } {
					set lsnfile [lindex $retval 0]
					set lsnoff [lindex $retval 1]
				}
				if { [is_substr $rettype NEWMASTER] } {
					set newmaster $retval
					# Break when we get a NEWMASTER message;
					# our caller needs to handle it.
					break
				}
			}

			if { $errorp != 0 } {
				# Break on an error, caller wants to handle it.
				break
			}
			if { $hold_elect == 1 } {
				# Break on a HOLDELECTION, for the same reason.
				break
			}
			if { $dupmaster == 1 } {
				# Break on a DUPMASTER, for the same reason.
				break
			}

		}
		error_check_good dbc_close [$dbc close] 0

		#
		# Check the number of keys remaining because we only
		# want to rename to done, message file that are
		# fully processed.  Some message types might break
		# out of the loop early and we want to process
		# the remaining messages the next time through.
		#
		set nkeys [stat_field $db stat "Number of keys"]
		error_check_good db_close [$db close] 0

		if { $nkeys == 0 } {
			set dbname [string replace $msgdb 0 5 done.]
#			file rename -force $queuedir/$msgdb $queuedir/$dbname
			file delete -force $queuedir/$msgdb
		}
	}
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
proc start_election \
    { pfx qdir envstring nsites nvotes pri timeout {err "none"} {crash 0}} {
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
	puts $oid "set is_repchild 1"
	puts $oid "replsetup $qdir"
	foreach i $machids { puts $oid "repladd $i" }
	puts $oid "set env_cmd \{$envstring\}"
	puts $oid "set dbenv \[eval \$env_cmd -errfile \
	    $testdir/ELECTION_ERRFILE.$elect_serial -errpfx $pfx \]"
#	puts $oid "set dbenv \[eval \$env_cmd -errfile \
#	    /dev/stdout -errpfx $pfx \]"
	puts $oid "\$dbenv test abort $err"
	puts $oid "set res \[catch \{\$dbenv rep_elect $nsites $nvotes $pri \
	    $elect_timeout\} ret\]"
	puts $oid "set r \[open \$testdir/ELECTION_RESULT.$elect_serial w\]"
	puts $oid "if \{\$res == 0 \} \{"
	puts $oid "puts \$r \"NEWMASTER \$ret\""
	puts $oid "\} else \{"
	puts $oid "puts \$r \"ERROR \$ret\""
	puts $oid "\}"
	#
	# This loop calls rep_elect a second time with the error cleared.
	# We don't want to do that if we are simulating a crash.
	if { $err != "none" && $crash != 1 } {
		puts $oid "\$dbenv test abort none"
		puts $oid "set res \[catch \{\$dbenv rep_elect $nsites \
		    $nvotes $pri $elect_timeout\} ret\]"
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

proc setpriority { priority nclients winner {start 0} } {
	upvar $priority pri

	for { set i $start } { $i < [expr $nclients + $start] } { incr i } {
		if { $i == $winner } {
			set pri($i) 100
		} else {
			set pri($i) 10
		}
	}
}

# run_election has the following arguments:
# Arrays:
#	ecmd 		Array of the commands for setting up each client env.
#	cenv		Array of the handles to each client env.
#	errcmd		Array of where errors should be forced.
#	priority	Array of the priorities of each client env.
#	crash		If an error is forced, should we crash or recover?
# The upvar command takes care of making these arrays available to
# the procedure.
#
# Ordinary variables:
# 	qdir		Directory where the message queue is located.
#	msg		Message prefixed to the output.
#	elector		This client calls the first election.
#	nsites		Number of sites in the replication group.
#	nvotes		Number of votes required to win the election.
# 	nclients	Number of clients participating in the election.
#	win		The expected winner of the election.
#	reopen		Should the new master (i.e. winner) be closed
#			and reopened as a client?
#	dbname		Name of the underlying database.  Defaults to
# 			the name of the db created by rep_test.
#
proc run_election { ecmd celist errcmd priority crsh qdir msg elector \
    nsites nvotes nclients win {reopen 0} {dbname "test.db"} } {
	global elect_timeout elect_serial
	global is_hp_test
	global is_windows_test
	global rand_init
	upvar $ecmd env_cmd
	upvar $celist cenvlist
	upvar $errcmd err_cmd
	upvar $priority pri
	upvar $crsh crash

	set elect_timeout 15000000

	foreach pair $cenvlist {
		set id [lindex $pair 1]
		set i [expr $id - 2]
		set elect_pipe($i) INVALID
		replclear $id
	}

	#
	# XXX
	# We need to somehow check for the warning if nvotes is not
	# a majority.  Problem is that warning will go into the child
	# process' output.  Furthermore, we need a mechanism that can
	# handle both sending the output to a file and sending it to
	# /dev/stderr when debugging without failing the
	# error_check_good check.
	#
	puts "\t\t$msg.1: Election with nsites=$nsites,\
	    nvotes=$nvotes, nclients=$nclients"
	puts "\t\t$msg.2: First elector is $elector,\
	    expected winner is $win (eid [expr $win + 2])"
	incr elect_serial
	set pfx "CHILD$elector.$elect_serial"
	# Windows and HP-UX require a longer timeout.
	if { $is_windows_test == 1 || $is_hp_test == 1 } {
		set elect_timeout [expr $elect_timeout * 2]
	}
	set elect_pipe($elector) [start_election \
	    $pfx $qdir $env_cmd($elector) $nsites $nvotes $pri($elector) \
	    $elect_timeout $err_cmd($elector) $crash($elector)]

	tclsleep 2

	set got_newmaster 0
	set tries [expr [expr $elect_timeout * 4] / 1000000]

	# If we're simulating a crash, skip the while loop and
	# just give the initial election a chance to complete.
	set crashing 0
	for { set i 0 } { $i < $nclients } { incr i } {
		if { $crash($i) == 1 } {
			set crashing 1
		}
	}

	if { $crashing == 1 } {
		tclsleep 10
	} else {
		while { 1 } {
			set nproced 0
			set he 0
			set nm 0
			set nm2 0

			foreach pair $cenvlist {
				set he 0
				set envid [lindex $pair 1]
				set i [expr $envid - 2]
				set clientenv($i) [lindex $pair 0]
				set child_done [check_election $elect_pipe($i) nm2]
				if { $got_newmaster == 0 && $nm2 != 0 } {
					error_check_good newmaster_is_master2 $nm2 \
					    [expr $win + 2]
					set got_newmaster $nm2

					# If this env is the new master, it needs to
					# configure itself as such--this is a different
					# env handle from the one that performed the
					# election.
					if { $nm2 == $envid } {
						error_check_good make_master($i) \
						    [$clientenv($i) rep_start -master] \
						    0
					}
				}
				incr nproced \
				    [replprocessqueue $clientenv($i) $envid 0 he nm]
# puts "Tries $tries: Processed queue for client $i, $nproced msgs he $he nm $nm nm2 $nm2"
				if { $he == 1 } {
					#
					# Only close down the election pipe if the
					# previously created one is done and
					# waiting for new commands, otherwise
					# if we try to close it while it's in
					# progress we hang this main tclsh.
					#
					if { $elect_pipe($i) != "INVALID" && \
					    $child_done == 1 } {
						close_election $elect_pipe($i)
						set elect_pipe($i) "INVALID"
					}
# puts "Starting election on client $i"
					if { $elect_pipe($i) == "INVALID" } {
						incr elect_serial
						set pfx "CHILD$i.$elect_serial"
						set elect_pipe($i) [start_election \
						    $pfx $qdir \
						    $env_cmd($i) $nsites \
						    $nvotes $pri($i) $elect_timeout]
						set got_hold_elect($i) 1
					}
				}
				if { $nm != 0 } {
					error_check_good newmaster_is_master $nm \
					    [expr $win + 2]
					set got_newmaster $nm

					# If this env is the new master, it needs to
					# configure itself as such--this is a different
					# env handle from the one that performed the
					# election.
					if { $nm == $envid } {
						error_check_good make_master($i) \
						    [$clientenv($i) rep_start -master] \
						    0
						# Occasionally force new log records
						# to be written.
						set write [berkdb random_int 1 10]
						if { $write == 1 } {
							set db [eval \
							    berkdb_open -env \
							    $clientenv($i) \
							    -auto_commit $dbname]
							error_check_good dbopen \
							    [is_valid_db $db] TRUE
							error_check_good dbclose \
							    [$db close] 0
						}
					}
				}
			}

			# We need to wait around to make doubly sure that the
			# election has finished...
			if { $nproced == 0 } {
				incr tries -1
				if { $tries == 0 } {
					break
				} else {
					tclsleep 1
				}
			} else {
				set tries $tries
			}
		}

		# Verify that expected winner is actually the winner.
		error_check_good "client $win wins" $got_newmaster [expr $win + 2]
	}

	cleanup_elections

	#
	# Make sure we've really processed all the post-election
	# sync-up messages.  If we're simulating a crash, don't process
	# any more messages.
	#
	if { $crashing == 0 } {
		process_msgs $cenvlist
	}

	if { $reopen == 1 } {
		puts "\t\t$msg.3: Closing new master and reopening as client"
		error_check_good newmaster_close [$clientenv($win) close] 0

		set clientenv($win) [eval $env_cmd($win)]
		error_check_good cl($win) [is_valid_env $clientenv($win)] TRUE
		set newelector "$clientenv($win) [expr $win + 2]"
		set cenvlist [lreplace $cenvlist $win $win $newelector]
		if { $crashing == 0 } {
			process_msgs $cenvlist
		}
	}
}

proc got_newmaster { cenv i newmaster win {dbname "test.db"} } {
	upvar $cenv clientenv

	# Check that the new master we got is the one we expected.
	error_check_good newmaster_is_master $newmaster [expr $win + 2]

	# If this env is the new master, it needs to configure itself
	# as such -- this is a different env handle from the one that
	# performed the election.
	if { $nm == $envid } {
		error_check_good make_master($i) \
		    [$clientenv($i) rep_start -master] 0
		# Occasionally force new log records to be written.
		set write [berkdb random_int 1 10]
		if { $write == 1 } {
			set db [eval berkdb_open -env $clientenv($i) \
			    -auto_commit -create -btree $dbname]
			error_check_good dbopen [is_valid_db $db] TRUE
			error_check_good dbclose [$db close] 0
		}
	}
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
	global qtestdir

	set t $elections_in_progress($i)
	puts $t "replclose \$qtestdir/MSGQUEUEDIR"
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
proc rep_test { method env repdb {nentries 10000} \
    {start 0} {skip 0} {needpad 0} {inmem 0} args } {

	source ./include.tcl

	#
	# Open the db if one isn't given.  Close before exit.
	#
	if { $repdb == "NULL" } {
		if { $inmem == 1 } {
			set testfile { "" "test.db" }
		} else {
			set testfile "test.db"
		}
		set largs [convert_args $method $args]
		set omethod [convert_method $method]
		set db [eval {berkdb_open_noerr} -env $env -auto_commit\
		    -create -mode 0644 $omethod $largs $testfile]
		error_check_good reptest_db [is_valid_db $db] TRUE
	} else {
		set db $repdb
	}

	puts "\t\tRep_test: $method $nentries key/data pairs starting at $start"
	set did [open $dict]

	# The "start" variable determines the record number to start
	# with, if we're using record numbers.  The "skip" variable
	# determines which dictionary entry to start with.  In normal
	# use, skip is equal to start.

	if { $skip != 0 } {
		for { set count 0 } { $count < $skip } { incr count } {
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

	# Checkpoint 10 times during the run, but not more
	# frequently than every 5 entries.
	set checkfreq [expr $nentries / 10]

	# Abort occasionally during the run.
	set abortfreq [expr $nentries / 15]

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
		#
		# We want to make sure we send in exactly the same
		# length data so that LSNs match up for some tests
		# in replication (rep021).
		#
		if { [is_fixed_length $method] == 1 && $needpad } {
			#
			# Make it something visible and obvious, 'A'.
			#
			set p 65
			set str [make_fixed_length $method $str $p]
			set kvals($key) $str
		}
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0
		error_check_good txn [$t commit] 0

		if { $checkfreq < 5 } {
			set checkfreq 5
		}
		if { $abortfreq < 3 } {
			set abortfreq 3
		}
		#
		# Do a few aborted transactions to test that
		# aborts don't get processed on clients and the
		# master handles them properly.  Just abort
		# trying to delete the key we just added.
		#
		if { $count % $abortfreq == 0 } {
			set t [$env txn]
			error_check_good txn [is_valid_txn $t $env] TRUE
			set ret [$db del -txn $t $key]
			error_check_good txn [$t abort] 0
		}
		if { $count % $checkfreq == 0 } {
			error_check_good txn_checkpoint($count) \
			    [$env txn_checkpoint] 0
		}
		incr count
	}
	close $did
	if { $repdb == "NULL" } {
		error_check_good rep_close [$db close] 0
	}
}

#
# This is essentially a copy of rep_test, but it only does the put/get
# loop in a long running txn to an open db.  We use it for bulk testing
# because we want to fill the bulk buffer some before sending it out.
# Bulk buffer gets transmitted on every commit.
#
proc rep_test_bulk { method env repdb {nentries 10000} \
    {start 0} {skip 0} {useoverflow 0} args } {
	source ./include.tcl

	global overflowword1
	global overflowword2

	if { [is_fixed_length $method] && $useoverflow == 1 } {
		puts "Skipping overflow for fixed length method $method"
		return
	}
	#
	# Open the db if one isn't given.  Close before exit.
	#
	if { $repdb == "NULL" } {
		set testfile "test.db"
		set largs [convert_args $method $args]
		set omethod [convert_method $method]
		set db [eval {berkdb_open_noerr -env $env -auto_commit -create \
		    -mode 0644} $largs $omethod $testfile]
		error_check_good reptest_db [is_valid_db $db] TRUE
	} else {
		set db $repdb
	}

	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	# If we are not using an external env, then test setting
	# the database cache size and using multiple caches.
	puts \
"\t\tRep_test_bulk: $method $nentries key/data pairs starting at $start"
	set did [open $dict]

	# The "start" variable determines the record number to start
	# with, if we're using record numbers.  The "skip" variable
	# determines which dictionary entry to start with.  In normal
	# use, skip is equal to start.

	if { $skip != 0 } {
		for { set count 0 } { $count < $skip } { incr count } {
			gets $did str
		}
	}
	set pflags ""
	set gflags ""
	set txn ""

	if { [is_record_based $method] == 1 } {
		append gflags " -recno"
	}
	puts "\t\tRep_test_bulk.a: put/get loop in 1 txn"
	# Here is the loop where we put and get each key/data pair
	set count 0

	set t [$env txn]
	error_check_good txn [is_valid_txn $t $env] TRUE
	set txn "-txn $t"
	set pid [pid]
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
			if { [is_fixed_length $method] == 0 } {
				set str [repeat $str 100]
			}
		} else {
			set key $str.$pid
			set str [repeat $str 100]
		}
		#
		# For use for overflow test.
		#
		if { $useoverflow == 0 } {
			if { [string length $overflowword1] < \
			    [string length $str] } {
				set overflowword2 $overflowword1
				set overflowword1 $str
			}
		} else {
			if { $count == 0 } {
				set len [string length $overflowword1]
				set word $overflowword1
			} else {
				set len [string length $overflowword2]
				set word $overflowword1
			}
			set rpt [expr 1024 * 1024 / $len]
			incr rpt
			set str [repeat $word $rpt]
		}
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0
		incr count
	}
	error_check_good txn [$t commit] 0
	close $did
	if { $repdb == "NULL" } {
		error_check_good rep_close [$db close] 0
	}
}

proc rep_test_upg { method env repdb {nentries 10000} \
    {start 0} {skip 0} {needpad 0} {inmem 0} args } {

	source ./include.tcl

	#
	# Open the db if one isn't given.  Close before exit.
	#
	if { $repdb == "NULL" } {
		if { $inmem == 1 } {
			set testfile { "" "test.db" }
		} else {
			set testfile "test.db"
		}
		set largs [convert_args $method $args]
		set omethod [convert_method $method]
		set db [eval {berkdb_open_noerr} -env $env -auto_commit\
		    -create -mode 0644 $omethod $largs $testfile]
		error_check_good reptest_db [is_valid_db $db] TRUE
	} else {
		set db $repdb
	}

	set pid [pid]
	puts "\t\tRep_test_upg($pid): $method $nentries key/data pairs starting at $start"
	set did [open $dict]

	# The "start" variable determines the record number to start
	# with, if we're using record numbers.  The "skip" variable
	# determines which dictionary entry to start with.  In normal
	# use, skip is equal to start.

	if { $skip != 0 } {
		for { set count 0 } { $count < $skip } { incr count } {
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

	# Checkpoint 10 times during the run, but not more
	# frequently than every 5 entries.
	set checkfreq [expr $nentries / 10]

	# Abort occasionally during the run.
	set abortfreq [expr $nentries / 15]

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
			#
			# With upgrade test, we run the same test several
			# times with the same database.  We want to have
			# some overwritten records and some new records.
			# Therefore append our pid to half the keys.
			#
			if { $count % 2 } {
				set key $str.$pid
			} else {
				set key $str
			}
			set str [reverse $str]
		}
		#
		# We want to make sure we send in exactly the same
		# length data so that LSNs match up for some tests
		# in replication (rep021).
		#
		if { [is_fixed_length $method] == 1 && $needpad } {
			#
			# Make it something visible and obvious, 'A'.
			#
			set p 65
			set str [make_fixed_length $method $str $p]
			set kvals($key) $str
		}
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
puts "rep_test_upg: put $count of $nentries: key $key, data $str"
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0
		error_check_good txn [$t commit] 0

		if { $checkfreq < 5 } {
			set checkfreq 5
		}
		if { $abortfreq < 3 } {
			set abortfreq 3
		}
		#
		# Do a few aborted transactions to test that
		# aborts don't get processed on clients and the
		# master handles them properly.  Just abort
		# trying to delete the key we just added.
		#
		if { $count % $abortfreq == 0 } {
			set t [$env txn]
			error_check_good txn [is_valid_txn $t $env] TRUE
			set ret [$db del -txn $t $key]
			error_check_good txn [$t abort] 0
		}
		if { $count % $checkfreq == 0 } {
			error_check_good txn_checkpoint($count) \
			    [$env txn_checkpoint] 0
		}
		incr count
	}
	close $did
	if { $repdb == "NULL" } {
		error_check_good rep_close [$db close] 0
	}
}

proc rep_test_upg.check { key data } {
	#
	# If the key has the pid attached, strip it off before checking.
	# If the key does not have the pid attached, then it is a recno
	# and we're done.
	#
	set i [string first . $key]
	if { $i != -1 } {
		set key [string replace $key $i end]
	}
	error_check_good "key/data mismatch" $data [reverse $key]
}

proc rep_test_upg.recno.check { key data } {
	#
	# If we're a recno database we better not have a pid in the key.
	# Otherwise we're done.
	#
	set i [string first . $key]
	error_check_good pid $i -1
}

proc process_msgs { elist {perm_response 0} {dupp NONE} {errp NONE} \
    {upg 0} } {
	if { $perm_response == 1 } {
		global perm_response_list
		set perm_response_list {{}}
	}

	if { [string compare $dupp NONE] != 0 } {
		upvar $dupp dupmaster
		set dupmaster 0
	} else {
		set dupmaster NONE
	}

	if { [string compare $errp NONE] != 0 } {
		upvar $errp errorp
		set errorp 0
	} else {
		set errorp NONE
	}

	set upgcount 0
	while { 1 } {
		set nproced 0
		incr nproced [proc_msgs_once $elist dupmaster errorp]
		#
		# If we're running the upgrade test, we are running only
		# our own env, we need to loop a bit to allow the other
		# upgrade procs to run and reply to our messages.
		#
		if { $upg == 1 && $upgcount < 10 } {
			tclsleep 2
			incr upgcount
			continue
		}
		if { $nproced == 0 } {
			break
		} else {
			set upgcount 0
		}
	}
}


proc proc_msgs_once { elist {dupp NONE} {errp NONE} } {
	if { [string compare $dupp NONE] != 0 } {
		upvar $dupp dupmaster
		set dupmaster 0
	} else {
		set dupmaster NONE
	}

	if { [string compare $errp NONE] != 0 } {
		upvar $errp errorp
		set errorp 0
	} else {
		set errorp NONE
	}

	set nproced 0
	foreach pair $elist {
		set envname [lindex $pair 0]
		set envid [lindex $pair 1]
		#
		# If we need to send in all the other args
		incr nproced [replprocessqueue $envname $envid \
		    0 NONE NONE dupmaster errorp]
		#
		# If the user is expecting to handle an error and we get
		# one, return the error immediately.
		#
		if { $dupmaster != 0 && $dupmaster != "NONE" } {
			return 0
		}
		if { $errorp != 0 && $errorp != "NONE" } {
			return 0
		}
	}
	return $nproced
}

proc rep_verify { masterdir masterenv clientdir clientenv {init_test 0} \
    {match 1} {logcompare 1} {dbname "test.db"} } {
	global util_path
	global encrypt
	global passwd

	# The logcompare flag indicates whether to compare logs.
	# Sometimes we run a test where rep_verify is run twice with
	# no intervening processing of messages.  If that test is
	# on a build with debug_rop enabled, the master's log is
	# altered by the first rep_verify, and the second rep_verify
	# will fail.
	# To avoid this, skip the log comparison on the second rep_verify
	# by specifying logcompare == 0.
	#
	if { $logcompare } {
		set msg "Logs and databases"
	} else {
		set msg "Databases ($dbname)"
	}

	if { $match } {
		puts "\t\tRep_verify: $clientdir: $msg match"
	} else {
		puts "\t\tRep_verify: $clientdir: $msg do not match"
	}
	# Check that master and client logs and dbs are identical.

	# Logs first, if specified ...
	#
	# If init_test is set, we need to run db_printlog on the log
	# subset that the client has.  The master may have more (earlier)
	# log files.
	#
	if { $logcompare } {
		set margs ""
		if { $init_test } {
	                set logc [$clientenv log_cursor]
			error_check_good logc [is_valid_logc $logc $clientenv] TRUE
			set first [$logc get -first]
			error_check_good close [$logc close] 0
			set lsn [lindex $first 0]
			set file [lindex $lsn 0]
			set off [lindex $lsn 1]
			set margs "-b $file/$off"
		}
		set encargs ""
		if { $encrypt == 1 } {
			set encargs " -P $passwd "
		}

		set stat [catch {eval exec $util_path/db_printlog $margs \
		    $encargs -h $masterdir > $masterdir/prlog} result]
		error_check_good stat_mprlog $stat 0
		set stat [catch {eval exec $util_path/db_printlog \
		    $encargs -h $clientdir > $clientdir/prlog} result]
		error_check_good stat_cprlog $stat 0
		if { $match } {
			error_check_good log_cmp \
			    [filecmp $masterdir/prlog $clientdir/prlog] 0
		} else {
			error_check_bad log_cmp \
			    [filecmp $masterdir/prlog $clientdir/prlog] 0
		}

		if { $dbname == "NULL" } {
			return
		}
	}

	# ... now the databases.
	set db1 [eval {berkdb_open_noerr} -env $masterenv -rdonly $dbname]
	set db2 [eval {berkdb_open_noerr} -env $clientenv -rdonly $dbname]

	if { $match } {
		error_check_good comparedbs [db_compare \
		    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	} else {
		error_check_bad comparedbs [db_compare \
		    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	}
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0
}

proc rep_startup_event { event } {
	global startup_done

	# puts "rep_startup_event: Got event $event"
	if { $event == "startupdone" } {
		set startup_done 1
	}
	return
}
