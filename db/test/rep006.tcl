# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003,2007 Oracle.  All rights reserved.
#
# $Id: rep006.tcl,v 12.15 2007/05/17 18:17:21 bostic Exp $
#
# TEST  rep006
# TEST	Replication and non-rep env handles.
# TEST
# TEST	Run a modified version of test001 in a replicated master
# TEST	environment; verify that the database on the client is correct.
# TEST	Next, create a non-rep env handle to the master env.
# TEST	Attempt to open the database r/w to force error.

proc rep006 { method { niter 1000 } { tnum "006" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}
	set logsets [create_logsets 2]

	# All access methods are allowed.
	if { $checking_valid_methods } {
		return "ALL"
	}

	# Run the body of the test with and without recovery.
	foreach r $test_recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping for in-memory logs\
				    with -recover."
				continue
			}
			puts "Rep$tnum ($method $r):\
			    Replication and non-rep env handles"
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep006_sub $method $niter $tnum $l $r $args
		}
	}
}

proc rep006_sub { method niter tnum logset recargs largs } {
	source ./include.tcl
	global testdir
	global is_hp_test
	global rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

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

	if { [is_record_based $method] == 1 } {
		set checkfunc test001_recno.check
	} else {
		set checkfunc test001.check
	}

	# Open a master.
	repladd 1
	set max_locks 2500
	set env_cmd(M) "berkdb_env_noerr -create -log_max 1000000 \
	    -lock_max_objects $max_locks -lock_max_locks $max_locks \
	    -home $masterdir -errpfx MASTER $verbargs \
	    $m_txnargs $m_logargs -rep_master -rep_transport \
	    \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M) $recargs]

	# Open a client
	repladd 2
	set env_cmd(C) "berkdb_env_noerr -create $c_txnargs $c_logargs \
	    -lock_max_objects $max_locks -lock_max_locks $max_locks \
	    -home $clientdir -errpfx CLIENT $verbargs \
	    -rep_client -rep_transport \[list 2 replsend\]"
	set clientenv [eval $env_cmd(C) $recargs]

	# Bring the client online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Run a modified test001 in the master (and update client).
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
	process_msgs $envlist

	# Verify the database in the client dir.
	puts "\tRep$tnum.b: Verifying client database contents."
	set testdir [get_home $masterenv]
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test.db $clientenv $t1 \
	    $checkfunc dump_file_direction "-first" "-next"

	# Determine whether this build is configured with --enable-debug_rop
	# or --enable-debug_wop; we'll need to skip portions of the test if so.
	set conf [berkdb getconfig]
	set debug_rop_wop 0
	if { [is_substr $conf "debug_rop"] == 1 \
	    || [is_substr $conf "debug_wop"] == 1 } {
		set debug_rop_wop 1
	}

	# Skip if configured with --enable-debug_rop or --enable-debug_wop,
	# because the checkpoint won't fail in those cases.
	if { $debug_rop_wop == 1 } {
		puts "\tRep$tnum.c: Skipping for debug_rop/debug_wop."
	} else {
		puts "\tRep$tnum.c: Verifying non-master db_checkpoint."
		set stat \
		    [catch {exec $util_path/db_checkpoint -h $masterdir -1} ret]
		error_check_good open_err $stat 1
		error_check_good \
		    open_err1 [is_substr $ret "attempting to modify"] 1
	}

	# We have to skip this bit for HP-UX because we can't open an env
	# twice, and for debug_rop/debug_wop because the open won't fail.
	if { $is_hp_test == 1 } {
		puts "\tRep$tnum.d: Skipping for HP-UX."
	} elseif { $debug_rop_wop == 1 } {
		puts "\tRep$tnum.d: Skipping for debug_rop/debug_wop."
	} else {
		puts "\tRep$tnum.d: Verifying non-master access."

		set rdenv [eval {berkdb_env_noerr} \
		    -home $masterdir $verbargs]
		error_check_good rdenv [is_valid_env $rdenv] TRUE
		#
		# Open the db read/write which will cause it to try to
		# write out a log record, which should fail.
		#
		set stat \
		    [catch {berkdb_open_noerr -env $rdenv test.db} ret]
		error_check_good open_err $stat 1
		error_check_good \
		    open_err1 [is_substr $ret "attempting to modify"] 1
		error_check_good rdenv_close [$rdenv close] 0
	}

	process_msgs $envlist

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0

	error_check_good verify \
	    [verify_dir $clientdir "\tRep$tnum.e: " 0 0 1] 0
	replclose $testdir/MSGQUEUEDIR
}
