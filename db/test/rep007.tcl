# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep007.tcl,v 11.15 2003/09/25 01:35:39 margo Exp $
#
# TEST  rep007
# TEST	Replication and bad LSNs
# TEST
# TEST	Run a modified version of test001 in a replicated master env.
# TEST  	Close the client.  Make additional changes to master.
# TEST  	Close the master.  Open the client as the new master.
# TEST 	Make several different changes.  Open the old master as
# TEST	the client.  Verify periodically that contents are correct.
proc rep007 { method { niter 10 } { tnum "007" } args } {
	global testdir

	puts "Rep$tnum: Replication and bad LSNs."
	set orig_tdir $testdir
	set largs $args

	set omethod [convert_method $method]

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
	set ma_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
#	set ma_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
#	    -verbose {rep on} \
#	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open two clients
	repladd 2
	set cl_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
#	set cl_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
#	    -verbose {rep on} \
#	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	repladd 3
	set cl2_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
#	set cl2_envcmd "berkdb_env -create -txn nosync -lock_max 2500 \
#	    -home $clientdir2 -rep_transport \[list 3 replsend\] \
#	    -verbose {rep on}"
	set cl2env [eval $cl2_envcmd -rep_client]
	error_check_good client2_env [is_valid_env $cl2env] TRUE

	# Bring the clients online by processing the startup messages.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}

	# Run a modified test001 in the master (and update clients).
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval test001 $method $niter 0 0 $tnum -env $masterenv $largs
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}

	# Databases should now have identical contents. We assume we
	# know the name of the file created by test001, "test$tnum.db".
	set dbname "test$tnum.db"
	if { [is_hash $method] == 0 } {
		set db1 [berkdb_open -env $masterenv -auto_commit $dbname]
		set db2 [berkdb_open -env $clientenv -auto_commit $dbname]
		set db3 [berkdb_open -env $cl2env -auto_commit $dbname]

		error_check_good compare1and2 \
		    [db_compare $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
		error_check_good compare1and3 \
		    [db_compare $db1 $db3 $masterdir/$dbname $clientdir2/$dbname] 0
		error_check_good db1_close [$db1 close] 0
		error_check_good db2_close [$db2 close] 0
		error_check_good db3_close [$db3 close] 0
	}

	puts "\tRep$tnum.b: Close client 1 and make master changes."
	error_check_good client_close [$clientenv close] 0

	# Change master and propagate changes to client 2.
	set start $niter
	eval test001 $method $niter $start 1 $tnum -env $masterenv $largs
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}

	# We need to do a deletion here to cause meta-page updates,
	# particularly for queue.  Delete the first pair and remember
	# what it is -- it should come back after the master is closed
	# and reopened as a client.
	set db1 [berkdb_open -env $masterenv -auto_commit $dbname]
	error_check_good dbopen [is_valid_db $db1] TRUE
	set txn [$masterenv txn]
	set c [$db1 cursor -txn $txn]
	error_check_good db_cursor [is_valid_cursor $c $db1] TRUE
	set first [$c get -first]
	set pair [lindex [$c get -first] 0]
	set key [lindex $pair 0]
	set data [lindex $pair 1]
	error_check_bad dbcget [llength $key] 0
	error_check_good cursor_del [$c del] 0
	error_check_good dbcclose [$c close] 0
	error_check_good txn_commit [$txn commit] 0
	error_check_good db1_close [$db1 close] 0
	#
	# Process the messages to get them out of the db.  This also
	# propagates the delete to client 2.
	#
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}
	# Nuke those for closed client
	replclear 2

	# Databases 1 and 3 should now have identical contents.
	# Database 2 should be different.  First check 1 and 3.  We
	# have to wait to check 2 until the env is open again.
	set db1 [berkdb_open -env $masterenv -auto_commit $dbname]
	set db3 [berkdb_open -env $cl2env -auto_commit $dbname]

	error_check_good compare1and3 \
	    [db_compare $db1 $db3 $masterdir/$dbname $clientdir2/$dbname] 0
	error_check_good db1_close [$db1 close] 0

	puts "\tRep$tnum.c: Close master, reopen client as master."
	error_check_good master_close [$masterenv close] 0

	set newmasterenv [eval $cl_envcmd -rep_master]
	# Now we can check that database 2 does not match 3.
	if { [is_hash $method] == 0 } {
		set db2 [berkdb_open -env $newmasterenv -auto_commit $dbname]

		catch {db_compare $db2 $db3 $clientdir/$dbname \
		    $clientdir2/$dbname} res
		error_check_bad compare2and3_must_fail $res 0
		error_check_good db2_close [$db2 close] 0
	}
	error_check_good db3_close [$db3 close] 0

	puts "\tRep$tnum.d: Make incompatible changes to new master."
	set db [berkdb_open -env $newmasterenv -auto_commit -create $omethod \
	    test007.db]
	error_check_good dbopen [is_valid_db $db] TRUE
	set t [$newmasterenv txn]
	# Force in a pair {10 10}.  This works for all access
	# methods and won't overwrite the old first pair for record-based.
	set ret [$db put -txn $t 10 [chop_data $method 10]]
	error_check_good put $ret 0
	error_check_good txn [$t commit] 0
	error_check_good dbclose [$db close] 0

	eval test001 $method $niter $start 1 $tnum -env $newmasterenv $largs
	set cl2rec 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newmasterenv 2]
		incr nproced [replprocessqueue $cl2env 3]
		# At some point in the processing, client2 should be
		# in recovery.
		set stat [$cl2env rep_stat]
		if { [is_substr $stat "{{In recovery} 1}"] } {
			set cl2rec 1
		}

		if { $nproced == 0 } {
			break
		}
	}

	# Nuke those for closed old master
	replclear 1

	#
	# Check that cl2 stats showed we were in recovery and now that
	# we're done, we should be out of it.
	#
	error_check_good cl2rec $cl2rec 1
	set stat [$cl2env rep_stat]
	error_check_good cl2recover [is_substr $stat "{{In recovery} 0}"] 1

	# Databases 2 and 3 should now match.
	set db2 [berkdb_open -env $newmasterenv -auto_commit $dbname]
	set db3 [berkdb_open -env $cl2env -auto_commit $dbname]

	error_check_good compare2and3 \
	    [db_compare $db2 $db3 $clientdir/$dbname $clientdir2/$dbname] 0
	error_check_good db2_close [$db2 close] 0
	error_check_good db3_close [$db3 close] 0

	puts "\tRep$tnum.e: Open old master as client."
	set newclientenv [eval $ma_envcmd -rep_client -recover]
	# Bring the newclient online by processing the startup messages.
	set ncrec 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newmasterenv 2]
		incr nproced [replprocessqueue $newclientenv 1]
		set stat [$newclientenv rep_stat]
		if { [is_substr $stat "{{In recovery} 1}"] } {
			set ncrec 1
		}
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}
	error_check_good ncrec $ncrec 1
	set stat [$newclientenv rep_stat]
	error_check_good nc2recover [is_substr $stat "{{In recovery} 0}"] 1

	# The pair we deleted earlier from the master should now
	# have reappeared.
	set db1 [berkdb_open -env $newclientenv -auto_commit $dbname]
	error_check_good dbopen [is_valid_db $db1] TRUE
	set ret [$db1 get -get_both $key [pad_data $method $data]]
	error_check_good get_both $ret [list $pair]
	error_check_good db1_close [$db1 close] 0

	set start [expr $niter * 2]
	eval test001 $method $niter $start 1 $tnum -env $newmasterenv $largs

	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newmasterenv 2]
		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $cl2env 3]

		if { $nproced == 0 } {
			break
		}
	}

	# Now all 3 should match again.
	set db1 [berkdb_open -env $newclientenv -auto_commit $dbname]
	set db2 [berkdb_open -env $newmasterenv -auto_commit $dbname]
	set db3 [berkdb_open -env $cl2env -auto_commit $dbname]

	error_check_good compare1and2 \
	    [db_compare $db1 $db2 $masterdir/$dbname $clientdir/$dbname] 0
	error_check_good compare1and3 \
	    [db_compare $db1 $db3 $masterdir/$dbname $clientdir2/$dbname] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0
	error_check_good db3_close [$db3 close] 0

	error_check_good newmasterenv_close [$newmasterenv close] 0
	error_check_good newclientenv_close [$newclientenv close] 0
	error_check_good cl2_close [$cl2env close] 0
	replclose $testdir/MSGQUEUEDIR
	set testdir $orig_tdir
	return
}
