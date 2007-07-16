# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep046.tcl,v 12.16 2006/09/07 14:48:25 carol Exp $
#
# TEST  rep046
# TEST	Replication and basic bulk transfer.
# TEST	Set bulk transfer replication option.
# TEST	Run long txns on master and then commit.  Process on client
# TEST	and verify contents.  Run a very long txn so that logging
# TEST	must send the log.  Process and verify on client.
#
proc rep046 { method { nentries 200 } { tnum "046" } args } {
	global mixed_mode_logging
	source ./include.tcl

	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win9x platform."
		return
	}

	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]

	# Run the body of the test with and without recovery.
	set throttle { "throttle" "" }
	foreach r $test_recopts {
		foreach t $throttle {
			puts "Rep$tnum ($method $r $t):\
			    Replication and bulk transfer."
			rep046_sub $method $nentries $tnum $r $t $args
		}
	}
}

proc rep046_sub { method niter tnum recargs throttle largs } {
	global overflowword1
	global overflowword2
	global testdir
	global util_path

	set orig_tdir $testdir
	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	file mkdir $masterdir
	file mkdir $clientdir

	set bigniter [expr 10000 - [expr 2 * $niter]]
	set lkmax [expr $bigniter * 2]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env -create -txn nosync \
	    -lock_max_locks 10000 -lock_max_objects 10000 \
	    -home $masterdir -rep_master -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env -create -txn nosync \
#	    -lock_max_locks 10000 -lock_max_objects 10000 \
#	    -errpfx MASTER -verbose {rep on} -errfile /dev/stderr \
#	    -home $masterdir -rep_master -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	repladd 2
	set cl_envcmd "berkdb_env -create -txn nosync \
	    -home $clientdir \
	    -lock_max_locks 10000 -lock_max_objects 10000 \
	    -rep_client -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env -create -txn nosync \
#	    -home $clientdir \
#	    -lock_max_locks 10000 -lock_max_objects 10000 \
#	    -errpfx CLIENT -verbose {rep on} -errfile /dev/stderr \
#	    -rep_client -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	if { $throttle == "throttle" } {
		set clientdir2 $testdir/CLIENTDIR2
		file mkdir $clientdir2
		repladd 3
		set cl2_envcmd "berkdb_env -create -txn nosync \
		    -home $clientdir2 \
	    	    -lock_max_locks 10000 -lock_max_objects 10000 \
		    -rep_client -rep_transport \[list 3 replsend\]"
#		set cl2_envcmd "berkdb_env -create -txn nosync \
#		    -home $clientdir2 \
#	    	    -lock_max_locks 10000 -lock_max_objects 10000 \
#		    -errpfx CLIENT2 -verbose {rep on} -errfile /dev/stderr \
#		    -rep_client -rep_transport \[list 3 replsend\]"
		set cl2env [eval $cl2_envcmd $recargs]
		error_check_good client2_env [is_valid_env $cl2env] TRUE
		set envlist "{$masterenv 1} {$clientenv 2} {$cl2env 3}"
		#
		# Turn throttling on in master
		#
		error_check_good thr [$masterenv rep_limit 0 [expr 32 * 1024]] 0
	} else {
		set envlist "{$masterenv 1} {$clientenv 2}"
	}
	# Bring the client online by processing the startup messages.
	process_msgs $envlist

	#
	# Turn on bulk processing now on the master.
	#
	error_check_good set_bulk [$masterenv rep_config {bulk on}] 0

	puts "\tRep$tnum.a: Create and open master database"
	set testfile "test.db"
	set omethod [convert_method $method]
	set masterdb [eval {berkdb_open_noerr -env $masterenv -auto_commit \
	    -create -mode 0644} $largs $omethod $testfile]
	error_check_good dbopen [is_valid_db $masterdb] TRUE

	# Process database.
	process_msgs $envlist

	# Run a modified test001 in the master (and update clients).
	puts "\tRep$tnum.b: Basic long running txn"
	set bulkrec1 [stat_field $masterenv rep_stat "Bulk records stored"]
	set bulkxfer1 [stat_field $masterenv rep_stat "Bulk buffer transfers"]

	set overflowword1 "0"
	set overflowword2 "0"
	rep_test_bulk $method $masterenv $masterdb $niter 0 0
	process_msgs $envlist
	set bulkrec2 [stat_field $masterenv rep_stat "Bulk records stored"]
	set bulkxfer2 [stat_field $masterenv rep_stat "Bulk buffer transfers"]
	error_check_good recstat [expr $bulkrec2 > $bulkrec1] 1
	error_check_good xferstat [expr $bulkxfer2 > $bulkxfer1] 1
	rep_verify $masterdir $masterenv $clientdir $clientenv

	puts "\tRep$tnum.c: Very long txn"
	set skip $niter
	set start $niter
	set orig $niter
	set bulkfill1 [stat_field $masterenv rep_stat "Bulk buffer fills"]
	rep_test_bulk $method $masterenv $masterdb $bigniter $start $skip
	set start [expr $niter + $bigniter]
	if { $throttle == "throttle" } {
		#
		# If we're throttling clear all messages from client 3
		# so that we force a huge gap that the client will have
		# to ask for to invoke a rerequest that throttles.
		#
		replclear 3
		set old_thr \
		    [stat_field $masterenv rep_stat "Transmission limited"]
	}
	process_msgs $envlist
	set bulkfill2 [stat_field $masterenv rep_stat "Bulk buffer fills"]
	error_check_good fillstat [expr $bulkfill2 > $bulkfill1] 1
	rep_verify $masterdir $masterenv $clientdir $clientenv

	puts "\tRep$tnum.d: Very large data"
	set bulkovf1 [stat_field $masterenv rep_stat "Bulk buffer overflows"]
	set bulkfill1 [stat_field $masterenv rep_stat "Bulk buffer fills"]
	#
	# Send in '2' exactly because we're sending in the flag to use
	# the overflow entries.  We have 2 overflow entries.
	# If it's fixed length, we can't overflow.  Induce throttling by
	# putting in a bunch more entries.
	if { [is_fixed_length $method] == 1 } {
		rep_test_bulk $method $masterenv $masterdb $niter $start $start 0
	} else {
		rep_test_bulk $method $masterenv $masterdb 2 0 0 1
	}
	process_msgs $envlist

	# Determine whether this build is configured with --enable-debug_rop
	# or --enable-debug_wop.
	set conf [berkdb getconfig]
	set debug_rop_wop 0
	if { [is_substr $conf "debug_rop"] == 1 \
	    || [is_substr $conf "debug_wop"] == 1 } {
		set debug_rop_wop 1
	}

	# Generally overflows cannot happen because large data gets
	# broken up into overflow pages, and none will be larger than
	# the buffer.  However, if we're configured for debug_rop/wop
	# then we record the data as is and will overflow.
	#
	set bulkovf2 [stat_field $masterenv rep_stat "Bulk buffer overflows"]
	set bulkfill2 [stat_field $masterenv rep_stat "Bulk buffer fills"]
	if { [is_fixed_length $method] == 0 } {
		error_check_good fillstat1 [expr $bulkfill2 > $bulkfill1] 1
		if { $debug_rop_wop == 1 } {
			error_check_good overflows [expr $bulkovf2 > $bulkovf1] 1
		} else {
			error_check_good no_overflows $bulkovf2 0
		}
	}


	# !!!
	# Turn off bulk processing now on the master.  We need to do
	# this because some configurations (like debug_rop/wop) will
	# generate log records when verifying the logs and databases.
	# We want to control processing those messages.
	#
	error_check_good set_bulk [$masterenv rep_config {bulk off}] 0
	rep_verify $masterdir $masterenv $clientdir $clientenv

	if { $throttle == "throttle" } {
		puts "\tRep$tnum.e: Verify throttling."
		set new_thr \
		    [stat_field $masterenv rep_stat "Transmission limited"]
		error_check_bad nthrottles1 $new_thr -1
		error_check_bad nthrottles0 $new_thr 0
		error_check_good nthrottles [expr $old_thr < $new_thr] 1
		process_msgs $envlist
		rep_verify $masterdir $masterenv $clientdir2 $cl2env
		error_check_good cclose [$cl2env close] 0
	}
	error_check_good dbclose [$masterdb close] 0
	error_check_good mclose [$masterenv close] 0
	error_check_good cclose [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
