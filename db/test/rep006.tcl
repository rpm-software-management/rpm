# See the file LICENSE for redistribution information.
#
# Copyright (c) 2006-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep006.tcl,v 11.7 2003/08/28 19:59:14 sandstro Exp $
#
# TEST  rep006
# TEST	Replication and non-rep env handles.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  verify that the database on the client is correct.
# TEST	Next, create a non-rep env handle to the master env.
# TEST	Attempt to open the database r/w to force error.

proc rep006 { method { niter 1000 } { tnum "006" } args } {
	global passwd
	global has_crypto

	puts "Rep$tnum: Replication and non-rep env handles"

	set envargs ""
	rep006_sub $method $niter $tnum $envargs $args
}

proc rep006_sub { method niter tnum envargs largs } {
	source ./include.tcl
	global testdir
	global encrypt

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	if { [is_record_based $method] == 1 } {
		set checkfunc test001_recno.check
	} else {
		set checkfunc test001.check
	}

	# Open a master.
	repladd 1
	set masterenv \
	    [eval {berkdb_env -create -lock_max 2500 -log_max 1000000} \
	    $envargs {-home $masterdir -txn nosync -rep_master -rep_transport \
	    [list 1 replsend]}]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set clientenv [eval {berkdb_env -create} $envargs -txn nosync \
	    -lock_max 2500 \
	    {-home $clientdir -rep_client -rep_transport [list 2 replsend]}]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the client online by processing the startup messages.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Run a modified test001 in the master (and update client).
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval test001 $method $niter 0 0 $tnum -env $masterenv $largs
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Verify the database in the client dir.
	puts "\tRep$tnum.b: Verifying client database contents."
	set testdir [get_home $masterenv]
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test$tnum.db $clientenv $t1 \
	    $checkfunc dump_file_direction "-first" "-next"

	puts "\tRep$tnum.c: Verifying non-master db_checkpoint."
	set stat [catch {exec $util_path/db_checkpoint -h $masterdir -1} ret]
	error_check_good open_err $stat 1
	error_check_good open_err1 [is_substr $ret "attempting to modify"] 1

	puts "\tRep$tnum.d: Verifying non-master access."
	set rdenv \
	    [eval {berkdb_env_noerr} $envargs {-home $masterdir}]
	error_check_good rdenv [is_valid_env $rdenv] TRUE
	#
	# Open the db read/write which will cause it to try to
	# write out a log record, which should fail.
	#
	set stat [catch {berkdb_open_noerr -env $rdenv test$tnum.db} ret]
	error_check_good open_err $stat 1
	error_check_good open_err1 [is_substr $ret "attempting to modify"] 1
	error_check_good rdenv_close [$rdenv close] 0

	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0

	error_check_good verify \
	    [verify_dir $clientdir "\tRep$tnum.e: " 0 0 1] 0
	replclose $testdir/MSGQUEUEDIR
}
