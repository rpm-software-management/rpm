# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep031.tcl,v 11.6 2004/10/04 18:15:14 sue Exp $
#
# TEST	rep031
# TEST	Test of internal initialization and blocked operations.
# TEST
# TEST	One master, one client.
# TEST	Put one more record to the master.
# TEST  Test that internal initialization block log_archive, rename, remove.
# TEST	Sleep 30+ seconds.
# TEST  Test that we can now log_archive, rename, remove.
#
proc rep031 { method { niter 200 } { tnum "031" } args } {

	set args [convert_args $method $args]

	# This test needs to set its own pagesize.
	set pgindex [lsearch -exact $args "-pagesize"]
        if { $pgindex != -1 } {
                puts "Rep$tnum: skipping for specific pagesizes"
                return
        }

	# Run the body of the test with and without recovery,
	# and with and without cleaning.
	set recopts { "" " -recover " }
	set cleanopts { clean noclean }
	foreach r $recopts {
		foreach c $cleanopts {
			puts "Rep$tnum ($method $r $c $args):\
			    Test of internal initialization."
			rep031_sub $method $niter $tnum $r $c $args
		}
	}
}

proc rep031_sub { method niter tnum recargs clean largs } {
	global testdir
	global util_path

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
	set log_buf [expr $pagesize * 2]
	set log_max [expr $log_buf * 4]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -verbose {rep on} -errpfx MASTER \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -verbose {rep on} -errpfx CLIENT \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs
	process_msgs $envlist

	puts "\tRep$tnum.b: Close client."
	error_check_good client_close [$clientenv close] 0

	# Run rep_test in the master (don't update client).
	puts "\tRep$tnum.c: Running rep_test in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs
	replclear 2

	puts "\tRep$tnum.d: Run db_archive on master."
	set res [eval exec $util_path/db_archive -l -h $masterdir]
	error_check_bad log.1.present [lsearch -exact $res log.0000000001] -1
	set res [eval exec $util_path/db_archive -d -h $masterdir]
	set res [eval exec $util_path/db_archive -l -h $masterdir]
	error_check_good log.1.gone [lsearch -exact $res log.0000000001] -1

	puts "\tRep$tnum.e: Reopen client ($clean)."
	if { $clean == "clean" } {
		env_cleanup $clientdir
	}
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist 0 NONE err
	if { $clean == "noclean" } {
		puts "\tRep$tnum.e.1: Trigger log request"
		#
		# When we don't clean, starting the client doesn't
		# trigger any events.  We need to generate some log
		# records so that the client requests the missing
		# logs and that will trigger it.
		#
		set entries 10
		eval rep_test $method $masterenv NULL $entries $niter 0 0 $largs
		process_msgs $envlist 0 NONE err
	}

	#
	# We have now forced an internal initialization.  Verify it is correct.
	#
	puts "\tRep$tnum.f: Verify logs and databases"
	# Check that master and client logs and dbs are identical.
	# Logs first ...
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $masterdir > $masterdir/prlog} result]
	error_check_good stat_mprlog $stat 0
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $clientdir > $clientdir/prlog} result]
	error_check_good stat_cprlog $stat 0
	error_check_good log_cmp \
	    [filecmp $masterdir/prlog $clientdir/prlog] 0

	# ... now the databases.
	set dbname "test.db"
	set db1 [eval {berkdb_open -env $masterenv} $largs {-rdonly $dbname}]
	set db2 [eval {berkdb_open -env $clientenv} $largs {-rdonly $dbname}]

	error_check_good comparedbs [db_compare \
	    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0

	#
	# Internal initializations disable certain operations on the master for
	# 30 seconds after the last init-related message is received
	# by the master.  Those operations are dbremove, dbrename and
	# log_archive (with removal).
	#
	puts "\tRep$tnum.g: Try to remove and rename the database."

	set old $dbname
	set new $dbname.new
	set stat [catch {$masterenv dbrename -auto_commit $old $new} ret]
	error_check_good rename_fail $stat 1
	error_check_good archive_err [is_substr $ret  "invalid"] 1
	set stat [catch {$masterenv dbremove -auto_commit $old} ret]
	error_check_good remove_fail $stat 1
	error_check_good archive_err [is_substr $ret  "invalid"] 1

	#
	# Need entries big enough to generate additional log files.
	# However, db_archive will not return an error, it will
	# just retain the log file.
	#
	set entries 200
	eval rep_test $method $masterenv NULL $entries $niter 0 0 $largs
	process_msgs $envlist 0 NONE err

	puts "\tRep$tnum.h: Try to db_archive."
	set res [eval exec $util_path/db_archive -l -h $masterdir]
	set first [lindex $res 0]
	set res [eval exec $util_path/db_archive -d -h $masterdir]
	set res [eval exec $util_path/db_archive -l -h $masterdir]
	error_check_bad log.gone [lsearch -exact $res $first] -1

	#
	# Sleep 32 seconds - The timeout is 30 seconds, but we need
	# to sleep a bit longer to make sure we cross the timeout.
	#
	set to 32
	puts "\tRep$tnum.i: Wait $to seconds to timeout"
	tclsleep $to
	puts "\tRep$tnum.j: Retry blocked operations after wait"
	set stat [catch {$masterenv dbrename -auto_commit $old $new} ret]
	error_check_good rename_work $stat 0
	set stat [catch {$masterenv dbremove -auto_commit $new} ret]
	error_check_good remove_work $stat 0
	process_msgs $envlist 0 NONE err

	set stat [catch {eval exec $util_path/db_archive -d -h $masterdir} ret]
	set res [eval exec $util_path/db_archive -l -h $masterdir]
	error_check_good log.gone [lsearch -exact $res $first] -1

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
