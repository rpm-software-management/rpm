# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep012.tcl,v 11.6 2003/11/18 14:21:17 sue Exp $
#
# TEST  rep012
# TEST	Replication and dead DB handles.
# TEST
# TEST	Run a modified version of test001 in a replicated master env.
# TEST  Make additional changes to master, but not to the client.
# TEST  Downgrade the master and upgrade the client with open db handles.
# TEST  Verify that the roll back on clients gives dead db handles.
proc rep012 { method { niter 10 } { tnum "012" } args } {
	global testdir

	puts "Rep$tnum: Replication and dead ($method) db handles."
	set orig_tdir $testdir
	set largs $args

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR.2
	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
	    -errpfx ENV0 \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
#	    -errpfx ENV0 -verbose {rep on} -errfile /dev/stderr \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set env0 [eval $ma_envcmd -rep_master]
	set masterenv $env0
	error_check_good master_env [is_valid_env $env0] TRUE

	# Open two clients
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
	    -errpfx ENV1 \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
#	    -errpfx ENV1 -verbose {rep on} -errfile /dev/stderr \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set env1 [eval $cl_envcmd -rep_client]
	set clientenv $env1
	error_check_good client_env [is_valid_env $env1] TRUE

	repladd 3
	set cl2_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
	    -errpfx ENV2 \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
#	set cl2_envcmd "berkdb_env_noerr -create -txn nosync -lock_max 2500 \
#	    -errpfx ENV2 -verbose {rep on} -errfile /dev/stderr \
#	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
	set cl2env [eval $cl2_envcmd -rep_client]
	error_check_good client2_env [is_valid_env $cl2env] TRUE

	set testfile "test$tnum.db"
	set largs [convert_args $method $args]
	set omethod [convert_method $method]
	set env0db [eval {berkdb_open_noerr -env $env0 -auto_commit \
	     -create -mode 0644} $largs $omethod $testfile]
	set masterdb $env0db
	error_check_good dbopen [is_valid_db $env0db] TRUE

	# Bring the clients online by processing the startup messages.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $env0 1]
		incr nproced [replprocessqueue $env1 2]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}

	set env1db [eval {berkdb_open_noerr -env $env1 -auto_commit \
	     -mode 0644} $largs $omethod $testfile]
	set clientdb $env1db
	error_check_good dbopen [is_valid_db $env1db] TRUE
	set env2db [eval {berkdb_open_noerr -env $cl2env -auto_commit \
	     -mode 0644} $largs $omethod $testfile]
	error_check_good dbopen [is_valid_db $env2db] TRUE

	# Run a modified test001 in the master (and update clients).
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval rep_test $method $masterenv $masterdb $niter 0 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $env0 1]
		incr nproced [replprocessqueue $env1 2]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}

	set nstart $niter
	puts "\tRep$tnum.b: Run test in master and client 2 only"
	eval rep_test $method $masterenv $masterdb $niter $nstart 1
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $env0 1]
		# Ignore those for $env1
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}
	# Nuke those for client about to become master.
	replclear 2
	tclsleep 3
	puts "\tRep$tnum.c: Swap envs"
	set tmp $masterenv
	set masterenv $clientenv
	set clientenv $tmp
	error_check_good downgrade [$clientenv rep_start -client] 0
	error_check_good upgrade [$masterenv rep_start -master] 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $env0 1]
		incr nproced [replprocessqueue $env1 2]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}
	#
	# At this point, env0 should have rolled back across a txn commit.
	# If we do any operation on env0db, we should get an error that
	# the handle is dead.
	puts "\tRep$tnum.d: Try to access db handle after rollback"
	set stat1 [catch {$env0db stat} ret1]
	error_check_good stat1 $stat1 1
	error_check_good dead1 [is_substr $ret1 DB_REP_HANDLE_DEAD] 1

	set stat3 [catch {$env2db stat} ret3]
	error_check_good stat3 $stat3 1
	error_check_good dead3 [is_substr $ret3 DB_REP_HANDLE_DEAD] 1

	puts "\tRep$tnum.e: Closing"
	error_check_good env0db [$env0db close] 0
	error_check_good env1db [$env1db close] 0
	error_check_good cl2db [$env2db close] 0
	error_check_good env0_close [$env0 close] 0
	error_check_good env1_close [$env1 close] 0
	error_check_good cl2_close [$cl2env close] 0
	replclose $testdir/MSGQUEUEDIR
	set testdir $orig_tdir
	return
}
