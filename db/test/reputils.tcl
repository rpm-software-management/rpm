# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001
#	Sleepycat Software.  All rights reserved.
#
# Id: reputils.tcl,v 11.2 2001/10/05 02:38:09 bostic Exp 
#
# Replication testing utilities

# Environment handle for the env containing the replication "communications
# structure" (really a CDB environment).
global queueenv

# Array of DB handles, one per machine ID, for the databases that contain
# messages.
global queuedbs
global machids

# Create a replication group for testing.
proc replsetup { queuedir } {
	global queueenv queuedbs machids

	file mkdir $queuedir
	set queueenv \
	    [berkdb env -create -cdb -home $queuedir]
	error_check_good queueenv [is_valid_env $queueenv] TRUE

	if { [info exists queuedbs] } {
		unset queuedbs
	}
	set machids {}

	return $queueenv
}

proc replsend { control rec fromid toid } {
	global queuedbs machids


	# XXX
	# -1 is DB_BROADCAST_MID
	if { $toid == -1 } {
		set machlist $machids
	} else {
		if { [info exists queuedbs($toid)] != 1 } {
			puts stderr "FAIL: replsend: machid $toid not found"
			return -1
		}
		set machlist [list $toid]
	}

	foreach m $machlist {
		# XXX should a broadcast include to "self"?
		if { $m == $fromid } {
			continue
		}

		set db $queuedbs($m)

		$db put -append [list $control $rec $fromid]
	}

	return 0
}

proc repladd { machid } {
	global queueenv queuedbs machids

	if { [info exists queuedbs($machid)] == 1 } {
		error "FAIL: repladd: machid $machid already exists"
	}

	set queuedbs($machid) \
	    [berkdb open -env $queueenv -create -recno repqueue$machid.db]
	error_check_good repqueue_create [is_valid_db $queuedbs($machid)] TRUE

	lappend machids $machid
}

proc replprocessqueue { dbenv machid } {
	global queuedbs

	set nproced 0

	set dbc [$queuedbs($machid) cursor -update]
	error_check_good process_dbc($machid) \
	    [is_valid_cursor $dbc $queuedbs($machid)] TRUE

	for { set dbt [$dbc get -first] } \
	    { [llength $dbt] != 0 } \
	    { set dbt [$dbc get -next] } {
		set data [lindex [lindex $dbt 0] 1]

		error_check_good process_message [$dbenv rep_process_message \
		    [lindex $data 2] [lindex $data 0] [lindex $data 1]] 0

		incr nproced

		$dbc del
	}

	# Return the number of messages processed.
	return $nproced
}
