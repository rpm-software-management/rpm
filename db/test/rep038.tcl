# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004,2007 Oracle.  All rights reserved.
#
# $Id: rep038.tcl,v 12.19 2007/05/17 18:17:21 bostic Exp $
#
# TEST	rep038
# TEST	Test of internal initialization and ongoing master updates.
# TEST
# TEST	One master, one client.
# TEST	Generate several log files.
# TEST	Remove old master log files.
# TEST	Delete client files and restart client.
# TEST	Put more records on master while initialization is in progress.
#
proc rep038 { method { niter 200 } { tnum "038" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}

	# Valid for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]

	# This test needs to set its own pagesize.
	set pgindex [lsearch -exact $args "-pagesize"]
        if { $pgindex != -1 } {
                puts "Rep$tnum: skipping for specific pagesizes"
                return
        }

	set logsets [create_logsets 2]

	# Run the body of the test with and without recovery,
	# and with and without cleaning.  Skip recovery with in-memory
	# logging - it doesn't make sense.
	set cleanopts { clean noclean }
	foreach r $test_recopts {
		foreach c $cleanopts {
			foreach l $logsets {
				set logindex [lsearch -exact $l "in-memory"]
				if { $r == "-recover" && $logindex != -1 } {
					puts "Skipping rep$tnum for -recover\
					    with in-memory logs."
					continue
				}
				puts "Rep$tnum ($method $c $r $args):\
				    Test of internal init with new records."
				puts "Rep$tnum: Master logs are [lindex $l 0]"
				puts "Rep$tnum: Client logs are [lindex $l 1]"
				rep038_sub $method $niter $tnum $l $r $c $args
			}
		}
	}
}

proc rep038_sub { method niter tnum logset recargs clean largs } {
	global testdir
	global util_path
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

	# Log size is small so we quickly create more than one.
	# The documentation says that the log file must be at least
	# four times the size of the in-memory log buffer.
	set pagesize 4096
	append largs " -pagesize $pagesize "
	set log_max [expr $pagesize * 8]

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]

	# In-memory logs cannot be used with -txn nosync.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs \
	    $m_logargs -log_max $log_max -errpfx MASTER $verbargs \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	$masterenv rep_limit 0 0

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs \
	    $c_logargs -log_max $log_max -errpfx CLIENT $verbargs \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	$clientenv rep_limit 0 0

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Clobber replication's 30-second anti-archive timer, which will have
	# been started by client sync-up internal init, so that we can do a
	# log_archive in a moment.
	#
	$masterenv test force noarchive_timeout

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	set start 0
	eval rep_test $method $masterenv NULL $niter $start $start 0 0 $largs
	incr start $niter
	process_msgs $envlist

	puts "\tRep$tnum.b: Close client."
	if { $c_logtype != "in-memory" } {
		set res [eval exec $util_path/db_archive -l -h $clientdir]
	}
	set last_client_log [get_logfile $clientenv last]
	error_check_good client_close [$clientenv close] 0

	set stop 0
	while { $stop == 0 } {
		# Run rep_test in the master (don't update client).
		puts "\tRep$tnum.c: Running rep_test in replicated env."
		eval rep_test\
		    $method $masterenv NULL $niter $start $start 0 0 $largs
		incr start $niter
		replclear 2

		puts "\tRep$tnum.d: Run db_archive on master."
		if { $m_logtype == "on-disk" } {
			set res \
			    [eval exec $util_path/db_archive -d -h $masterdir]
		}
		# Make sure we have a gap between the last client log
		# and the first master log.
		set first_master_log [get_logfile $masterenv first]
		if { $first_master_log > $last_client_log } {
			set stop 1
		}

	}

	puts "\tRep$tnum.e: Reopen client ($clean)."
	if { $clean == "clean" } {
		env_cleanup $clientdir
	}
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE
	$clientenv rep_limit 0 0
	set envlist "{$masterenv 1} {$clientenv 2}"
	#
	# We want to simulate a master continually getting new
	# records while an update is going on.  Simulate that
	# for several iterations and then let the messages finish
	# all their processing.
	#
	set loop 10
	set i 0
	set entries 10
	set start $niter
	while { $i < $loop } {
		set nproced 0
		set start [expr $start + $entries]
		eval rep_test \
		    $method $masterenv NULL $entries $start $start 0 0 $largs
		incr start $entries
		incr nproced [proc_msgs_once $envlist NONE err]
		error_check_bad nproced $nproced 0
		incr i
	}
	process_msgs $envlist

	puts "\tRep$tnum.f: Verify logs and databases"
	rep_verify $masterdir $masterenv $clientdir $clientenv 1

	# Add records to the master and update client.
	puts "\tRep$tnum.g: Add more records and check again."
	eval rep_test $method $masterenv NULL $entries $start $start 0 0 $largs
	incr start $entries
	process_msgs $envlist 0 NONE err

	# Check again that master and client logs and dbs are identical.
	rep_verify $masterdir $masterenv $clientdir $clientenv 1

	# Make sure log file are on-disk or not as expected.
	check_log_location $masterenv
	check_log_location $clientenv

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
