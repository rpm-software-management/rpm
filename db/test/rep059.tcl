# See the file LICENSE for redistribution information.
#
# Copyright (c) 2005,2007 Oracle.  All rights reserved.
#
# $Id: rep059.tcl,v 1.12 2007/05/17 18:17:21 bostic Exp $
#
# TEST	rep059
# TEST
# TEST	Replication with multiple recycle records.
# TEST
# TEST	Mimic an application where a client has multiple recycle records
# TEST	only in its log and then tries to synchronize.  This has been
# TEST	a problem because there is real log, but no perm records to
# TEST	match on.
#
proc rep059 { method { tnum "059" } args } {
	source ./include.tcl
	global rep_verbose

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	set orig_tdir $testdir

	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}
	# There should be no difference with methods.  Just use btree.
	#
	if { $checking_valid_methods } {
		set test_methods { btree }
		return $test_methods
	}
	if { [is_btree $method] == 0 } {
		puts "Rep059: Skipping for method $method."
		return
	}

	set largs [convert_args $method $args]

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR2

	env_cleanup $testdir
	replsetup $testdir/MSGQUEUEDIR
	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	set omethod [convert_method $method]

	# Open a master.
	repladd 1
	set envcmd(M) "berkdb_env_noerr -create -txn nosync\
	    -lock_detect default -errpfx MASTER $verbargs \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set menv [eval $envcmd(M)]

	# Open a client
	repladd 2
	set envcmd(C) "berkdb_env_noerr -create -txn nosync \
	    -lock_detect default -errpfx CLIENT1 $verbargs \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set cenv [eval $envcmd(C)]

	# Open a 2nd client
	repladd 3
	set envcmd(C2) "berkdb_env_noerr -create -txn nosync \
	    -lock_detect default -errpfx CLIENT2 $verbargs \
	    -home $clientdir2 -rep_transport \[list 3 replsend\]"
	set c2env [eval $envcmd(C2)]

	#
	# Set test location, then start as master and client
	# This test hook will cause the master to return after
	# writing the txn_recycle record in rep_start, but
	# before writing the checkpoint, so that we have some
	# log, but no perm records in the log when the new
	# master takes over.
	#
	$menv test copy recycle

	puts "\tRep$tnum.a: Start master and 2 clients."
	error_check_good master [$menv rep_start -master] 0
	error_check_good client [$cenv rep_start -client] 0
	error_check_good client2 [$c2env rep_start -client] 0

	set envlist "{$menv 1} {$cenv 2} {$c2env 3}"
	process_msgs $envlist

	# Pretend master crashes.  Just close it and
	# don't use it anymore.
	error_check_good menv_close [$menv close] 0

	puts "\tRep$tnum.b: Make client1 master."
	set cenv [eval $envcmd(C) -rep_master]
	set envlist "{$cenv 2} {$c2env 3}"
	process_msgs $envlist

	puts "\tRep$tnum.c: Clean up."

	error_check_good cenv_close [$cenv close] 0
	error_check_good c2env_close [$c2env close] 0

	replclose $testdir/MSGQUEUEDIR
	set testdir $orig_tdir
	return
}

