# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep011.tcl,v 1.3 2003/09/26 16:05:13 sandstro Exp $
#
# TEST	rep011
# TEST	Replication: test open handle across an upgrade.
# TEST
# TEST	Open and close test database in master environment. 
# TEST	Update the client.  Check client, and leave the handle
# TEST 	to the client open as we close the masterenv and upgrade
# TEST	the client to master.  Reopen the old master as client
# TEST	and catch up.  Test that we can still do a put to the 
# TEST	handle we created on the master while it was still a 
# TEST	client, and then make sure that the change can be 
# TEST 	propagated back to the new client. 

proc rep011 { method { tnum "011" } args } {
	global passwd

	puts "Rep$tnum.a: Test upgrade of open handles ($method)."

	set envargs ""
	rep011_sub $method $tnum $envargs $args

	puts "Rep$tnum.b: Open handle upgrade test with encryption ($method)."
	append envargs " -encryptaes $passwd "
	append args " -encrypt "
	rep011_sub $method $tnum $envargs $args
}

proc rep011_sub { method tnum envargs largs } {
	source ./include.tcl
	global testdir
	global encrypt

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

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

	# Open a test database on the master so we can test having
	# handles open across an upgrade.
	puts "\tRep$tnum.a:\
	    Opening test database for post-upgrade client logging test."
	set master_upg_db [berkdb_open \
	    -create -auto_commit -btree -env $masterenv rep$tnum-upg.db]
	set puttxn [$masterenv txn]
	error_check_good master_upg_db_put \
	    [$master_upg_db put -txn $puttxn hello world] 0
	error_check_good puttxn_commit [$puttxn commit] 0
	error_check_good master_upg_db_close [$master_upg_db close] 0

	# Update the client.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Open the cross-upgrade database on the client and check its contents.
	set client_upg_db [berkdb_open \
	     -create -auto_commit -btree -env $clientenv rep$tnum-upg.db]
	error_check_good client_upg_db_get [$client_upg_db get hello] \
	     [list [list hello world]]
	# !!! We use this handle later.  Don't close it here.

	# Close master.
	puts "\tRep$tnum.b: Close master."
	error_check_good masterenv_close [$masterenv close] 0

	puts "\tRep$tnum.c: Upgrade client."
	set newmasterenv $clientenv
	error_check_good upgrade_client [$newmasterenv rep_start -master] 0

	puts "\tRep$tnum.d: Reopen old master as client and catch up."
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

	# Test put to the database handle we opened back when the new master
	# was a client.
	puts "\tRep$tnum.e: Test put to handle opened before upgrade."
	set puttxn [$newmasterenv txn]
	error_check_good client_upg_db_put \
	    [$client_upg_db put -txn $puttxn hello there] 0
	error_check_good puttxn_commit [$puttxn commit] 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Close the new master's handle for the upgrade-test database;  we
	# don't need it.  Then check to make sure the client did in fact
	# update the database.
	puts "\tRep$tnum.f: Test that client did update the database."
	error_check_good client_upg_db_close [$client_upg_db close] 0
	set newclient_upg_db [berkdb_open -env $newclientenv rep$tnum-upg.db]
	error_check_good newclient_upg_db_get [$newclient_upg_db get hello] \
	    [list [list hello there]]
	error_check_good newclient_upg_db_close [$newclient_upg_db close] 0

	error_check_good newmasterenv_close [$newmasterenv close] 0
	error_check_good newclientenv_close [$newclientenv close] 0

	if { [lsearch $envargs "-encrypta*"] !=-1 } {
		set encrypt 1
	}
	error_check_good verify \
	    [verify_dir $clientdir "\tRep$tnum.g: " 0 0 1] 0
	replclose $testdir/MSGQUEUEDIR
}
