# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep030.tcl,v 11.9 2004/10/07 18:24:11 carol Exp $
#
# TEST	rep030
# TEST	Test of internal initialization multiple files and pagesizes.
# TEST  Hold some databases open on master.
# TEST
# TEST	One master, one client.
# TEST	Generate several log files.
# TEST	Remove old master log files.
# TEST	Delete client files and restart client.
# TEST	Put one more record to the master.
#
proc rep030 { method { niter 500 } { tnum "030" } args } {
	set args [convert_args $method $args]

	# This test needs to set its own pagesize.
	set pgindex [lsearch -exact $args "-pagesize"]
        if { $pgindex != -1 } {
                puts "Rep$tnum: skipping for specific pagesizes"
                return
        }

	# Run the body of the test with and without recovery,
	# and with and without cleaning.
	set recopts { " -recover " "" }
	set cleanopts { noclean clean }
	foreach r $recopts {
		foreach c $cleanopts {
			puts "Rep$tnum ($method $r $c):\
			    Test of internal initialization."
			rep030_sub $method $niter $tnum $r $c $args
		}
	}
}

proc rep030_sub { method niter tnum recargs clean largs } {
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
	set maxpg 16384
	set log_buf [expr $maxpg * 2]
	set log_max [expr $log_buf * 4]
	set cache [expr $maxpg * 32 ]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -cachesize { 0 $cache 1 } \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -cachesize { 0 $cache 1 }\
#	    -verbose {rep on} -errpfx MASTER \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create -txn nosync \
	    -log_buffer $log_buf -log_max $log_max \
	    -cachesize { 0 $cache 1 }\
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create -txn nosync \
#	    -log_buffer $log_buf -log_max $log_max \
#	    -cachesize { 0 $cache 1 }\
#	    -verbose {rep on} -errpfx CLIENT \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Run rep_test in the master (and update client).
	set startpgsz 512
	set pglist ""
	for { set pgsz $startpgsz } { $pgsz <= $maxpg } \
	    { set pgsz [expr $pgsz * 2] } {
		lappend pglist $pgsz
	}
	set nfiles [llength $pglist]
	puts "\tRep$tnum.a.0: Running rep_test $nfiles times in replicated env."
	set dbopen ""
	for { set i 0 } { $i < $nfiles } { incr i } {
		set mult [expr $i * 10]
		set nentries [expr $niter + $mult]
		set pagesize [lindex $pglist $i]
		set largs " -pagesize $pagesize "
		eval rep_test $method $masterenv NULL $nentries $mult $mult \
		    0 $largs
		process_msgs $envlist

		#
		# Everytime we run 'rep_test' we create 'test.db'.  So
		# rename it each time through the loop.
		#
		set old "test.db"
		set new "test.$i.db"
		error_check_good rename [$masterenv dbrename \
		    -auto_commit $old $new] 0
		process_msgs $envlist
		#
		# We want to keep some databases open so that we test the
		# code finding the files in the data dir as well as finding
		# them in dbreg list.
		#
		if { [expr $i % 2 ] == 0 } {
			set db [berkdb_open -env $masterenv $new]
			error_check_good dbopen.$i [is_valid_db $db] TRUE
			lappend dbopen $db
		}
	}
	#
	# Set up a few special databases too.  We want one with a subdatabase
	# and we want an empty database.
	#
	set testfile "test.db"
	if { [is_queue $method] } {
		set sub ""
	} else {
		set sub "subdb"
	}
	set omethod [convert_method $method]
	set largs " -pagesize $maxpg "
	set largs [convert_args $method $largs]
	set emptyfile "empty.db"
	#
	# Create/close an empty database.
	#
	set db [eval {berkdb_open_noerr -env $masterenv -auto_commit -create \
	    -mode 0644} $largs $omethod $emptyfile]
	error_check_good emptydb [is_valid_db $db] TRUE
	error_check_good empty_close [$db close] 0
	#
	# Keep this subdb (regular if queue) database open. 
	# We need it a few times later on.
	#
	set db [eval {berkdb_open_noerr -env $masterenv -auto_commit -create \
	    -mode 0644} $largs $omethod $testfile $sub]
	error_check_good subdb [is_valid_db $db] TRUE
	eval rep_test $method $masterenv $db $niter 0 0 0 
	process_msgs $envlist

	puts "\tRep$tnum.b: Close client."
	error_check_good client_close [$clientenv close] 0

	#
	# Run rep_test in the master (don't update client).
	# Need to guarantee that we will change log files during
	# this run so run with the largest pagesize and double
	# the number of entries.
	#
	puts "\tRep$tnum.c: Running rep_test ( $largs) in replicated env."
	set nentries [expr $niter * 2]
	eval rep_test $method $masterenv $db $nentries 0 0 0
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
		set entries 100
		eval rep_test $method $masterenv $db $entries $niter 0 0
		process_msgs $envlist 0 NONE err
	}
	error_check_good subdb_close [$db close] 0
	process_msgs $envlist 0 NONE err

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
	set db1 [eval {berkdb_open -env $masterenv -rdonly} $dbname $sub]
	set db2 [eval {berkdb_open -env $clientenv -rdonly} $dbname $sub]

	error_check_good comparedbs [db_compare \
	    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0

	for { set i 0 } { $i < $nfiles } { incr i } {
		set dbname "test.$i.db"
		set db1 [berkdb_open -env $masterenv -rdonly $dbname]
		set db2 [berkdb_open -env $clientenv -rdonly $dbname]

		error_check_good comparedbs [db_compare \
		    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
		error_check_good db1_close [$db1 close] 0
		error_check_good db2_close [$db2 close] 0
	}

	#
	# Close the database held open on master for initialization.
	#
	foreach db $dbopen {
		error_check_good db_close [$db close] 0
	}

	# Add records to the master and update client.
	puts "\tRep$tnum.g: Add more records and check again."
	set entries 10
	set db [eval {berkdb_open_noerr -env $masterenv -auto_commit \
	    -mode 0644} $largs $omethod $testfile $sub]
	error_check_good subdb [is_valid_db $db] TRUE
	eval rep_test $method $masterenv $db $entries $niter 0 0
	error_check_good subdb_close [$db close] 0
	process_msgs $envlist 0 NONE err

	# Check again that master and client logs and dbs are identical.
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $masterdir > $masterdir/prlog} result]
	error_check_good stat_mprlog $stat 0
	set stat [catch {eval exec $util_path/db_printlog \
	    -h $clientdir > $clientdir/prlog} result]
	error_check_good stat_cprlog $stat 0
	error_check_good log_cmp \
	    [filecmp $masterdir/prlog $clientdir/prlog] 0

	set dbname "test.db"
	set db1 [eval {berkdb_open -env $masterenv -auto_commit} $dbname $sub]
	set db2 [eval {berkdb_open -env $clientenv -auto_commit} $dbname $sub]

	error_check_good comparedbs [db_compare \
	    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0

	for { set i 0 } { $i < $nfiles } { incr i } {
		set dbname "test.$i.db"
		set db1 [berkdb_open -env $masterenv -auto_commit $dbname]
		set db2 [berkdb_open -env $clientenv -auto_commit $dbname]

		error_check_good comparedbs [db_compare \
		    $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
		error_check_good db1_close [$db1 close] 0
		error_check_good db2_close [$db2 close] 0
	}

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
