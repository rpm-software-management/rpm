# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep001.tcl,v 1.23 2003/09/04 23:41:12 bostic Exp $
#
# TEST  rep001
# TEST	Replication rename and forced-upgrade test.
# TEST
# TEST	Run a modified version of test001 in a replicated master
# TEST	environment; verify that the database on the client is correct.
# TEST	Next, remove the database, close the master, upgrade the
# TEST	client, reopen the master, and make sure the new master can
# TEST	correctly run test001 and propagate it in the other direction.

proc rep001 { method { niter 1000 } { tnum "001" } args } {
	global passwd
	global has_crypto

	puts "Rep$tnum: Replication sanity test."

	set envargs ""
	rep001_sub $method $niter $tnum $envargs $args

	# Skip remainder of test if release does not support encryption.
	if { $has_crypto == 0 } {
		return
	}

	puts "Rep$tnum: Replication and security sanity test."
	append envargs " -encryptaes $passwd "
	append args " -encrypt "
	rep001_sub $method $niter $tnum $envargs $args
}

proc rep001_sub { method niter tnum envargs largs } {
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
	    -lock_max 2500 {-home $clientdir -rep_client -rep_transport \
	    [list 2 replsend]}]
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

	# Verify the database in the client dir.  We assume we know
	# the name of the database created by test001 -- it is
	# test$tnum.db.
	puts "\tRep$tnum.b: Verifying client database contents."
	set testdir [get_home $masterenv]
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test$tnum.db $clientenv $t1 \
	    $checkfunc dump_file_direction "-first" "-next"

	# Remove the file (and update client).
	puts "\tRep$tnum.c: Remove the file on the master and close master."
	error_check_good remove \
	    [$masterenv dbremove -auto_commit test$tnum.db] 0
	error_check_good masterenv_close [$masterenv close] 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	puts "\tRep$tnum.d: Upgrade client."
	set newmasterenv $clientenv
	error_check_good upgrade_client [$newmasterenv rep_start -master] 0

	# Run test001 in the new master
	puts "\tRep$tnum.e: Running test001 in new master."
	eval test001 $method $niter 0 0 $tnum -env $newmasterenv $largs
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	puts "\tRep$tnum.f: Reopen old master as client and catch up."
	# Throttle master so it can't send everything at once
	$newmasterenv rep_limit 0 [expr 64 * 1024]
	set newclientenv [eval {berkdb_env -create -recover} $envargs \
	    -txn nosync -lock_max 2500 \
	    {-home $masterdir -rep_client -rep_transport [list 1 replsend]}]
	error_check_good newclient_env [is_valid_env $newclientenv] TRUE
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}
	set stats [$newmasterenv rep_stat]
	set nthrottles [getstats $stats {Transmission limited}]
	error_check_bad nthrottles $nthrottles -1
	error_check_bad nthrottles $nthrottles 0

	# Run a modified test001 in the new master (and update client).
	puts "\tRep$tnum.g: Running test001 in new master."
	eval test001 $method \
	    $niter $niter 1 $tnum -env $newmasterenv $largs
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Verify the database in the client dir.
	puts "\tRep$tnum.h: Verifying new client database contents."
	set testdir [get_home $newmasterenv]
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test$tnum.db $newclientenv $t1 \
	    $checkfunc dump_file_direction "-first" "-next"

	if { [string compare [convert_method $method] -recno] != 0 } {
		filesort $t1 $t3
	}
	error_check_good diff_files($t2,$t3) [filecmp $t2 $t3] 0


	error_check_good newmasterenv_close [$newmasterenv close] 0
	error_check_good newclientenv_close [$newclientenv close] 0

	if { [lsearch $envargs "-encrypta*"] !=-1 } {
		set encrypt 1
	}
	error_check_good verify \
	    [verify_dir $clientdir "\tRep$tnum.k: " 0 0 1] 0
	replclose $testdir/MSGQUEUEDIR
}
