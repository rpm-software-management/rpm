# See the file LICENSE for redistribution information.
#
# Copyright (c) 2002-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep002.tcl,v 11.25 2003/10/14 14:38:34 sandstro Exp $
#
# TEST  rep002
# TEST	Basic replication election test.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  hold an election among a group of clients to make sure they select
# TEST  a proper master from amongst themselves, in various scenarios.

proc rep002 { method { niter 10 } { nclients 3 } { tnum "002" } args } {
	source ./include.tcl
	global elect_timeout elect_serial

	set elect_timeout 5000000

	if { [is_record_based $method] == 1 } {
		puts "Rep002: Skipping for method $method."
		return
	}

	env_cleanup $testdir

	set qdir $testdir/MSGQUEUEDIR
	replsetup $qdir

	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir

	for { set i 0 } { $i < $nclients } { incr i } {
		set clientdir($i) $testdir/CLIENTDIR.$i
		file mkdir $clientdir($i)
	}

	puts "Rep$tnum: Replication election test with $nclients clients."

	# Open a master.
	repladd 1
	set env_cmd(M) "berkdb_env_noerr -create -log_max 1000000 \
	    -home $masterdir -errpfx MASTER \
	    -txn nosync -rep_master -rep_transport \[list 1 replsend\]"
#	set env_cmd(M) "berkdb_env_noerr -create -log_max 1000000 \
#	    -home $masterdir -errpfx MASTER -errfile /dev/stderr \
#	    -verbose {rep on} -txn nosync -rep_master \
#	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M)]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open the clients.
	for { set i 0 } { $i < $nclients } { incr i } {
		set envid [expr $i + 2]
		repladd $envid
		set env_cmd($i) "berkdb_env_noerr -create -home $clientdir($i) \
		    -txn nosync -rep_client -errpfx CLIENT$i \
		    -rep_transport \[list $envid replsend\]"
#		set env_cmd($i) "berkdb_env_noerr -create -home $clientdir($i) \
#		    -verbose {rep on} -errfile /dev/stderr \
#		    -txn nosync -rep_client -errpfx CLIENT$i \
#		    -rep_transport \[list $envid replsend\]"
		set clientenv($i) [eval $env_cmd($i)]
		error_check_good \
		    client_env($i) [is_valid_env $clientenv($i)] TRUE
	}

	# Run a modified test001 in the master.
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval test001 $method $niter 0 0 $tnum -env $masterenv $args

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]

		for { set i 0 } { $i < $nclients } { incr i } {
			set envid [expr $i + 2]
			incr nproced [replprocessqueue $clientenv($i) $envid]
		}

		if { $nproced == 0 } {
			break
		}
	}

	# Verify the database in the client dir.
	for { set i 0 } { $i < $nclients } { incr i } {
		puts "\tRep$tnum.b: Verifying contents of client database $i."
		set testdir [get_home $masterenv]
		set t1 $testdir/t1
		set t2 $testdir/t2
		set t3 $testdir/t3
		open_and_dump_file test$tnum.db $clientenv($i) $testdir/t1 \
	    	    test001.check dump_file_direction "-first" "-next"

		if { [string compare [convert_method $method] -recno] != 0 } {
			filesort $t1 $t3
		}
		error_check_good diff_files($t2,$t3) [filecmp $t2 $t3] 0

		verify_dir $clientdir($i) "\tRep$tnum.c: " 0 0 1
	}

	# Start an election in the first client.
	puts "\tRep$tnum.d: Starting election with existing master."
	# We want to verify that the master declares the election
	# over by fiat, even if everyone uses a lower priority than 20.
	# Loop and process all messages, keeping track of which
	# sites got a HOLDELECTION and checking that the returned newmaster,
	# if any, is 1 (the master's replication ID).
	set got_hold_elect(M) 0
	for { set i 0 } { $i < $nclients } { incr i } {
		set got_hold_elect($i) 0
		set elect_pipe($i) INVALID
	}
	set elect_pipe(0) [start_election C0 \
	    $qdir $env_cmd(0) [expr $nclients + 1] 20 $elect_timeout]

	tclsleep 2

	set got_master 0
	while { 1 } {
		set nproced 0
		set he 0
		set nm 0
		set nm2 0


		incr nproced [replprocessqueue $masterenv 1 0 he nm]

		if { $he == 1 } {
			incr elect_serial
			set elect_pipe(M) [start_election CM $qdir \
			    $env_cmd(M) [expr $nclients + 1] 0 $elect_timeout]
			set got_hold_elect(M) 1
		}
		if { $nm != 0 } {
			error_check_good newmaster_is_master $nm 1
			set got_master $nm
		}
		if { $nm2 != 0 } {
			error_check_good newmaster_is_master $nm2 1
			set got_master $nm2
		}

		for { set i 0 } { $i < $nclients } { incr i } {
			set he 0
			set envid [expr $i + 2]
			incr nproced \
			    [replprocessqueue $clientenv($i) $envid 0 he nm]
			set child_done [check_election $elect_pipe($i) nm2]
			if { $he == 1 } {
				# error_check_bad client(0)_in_elect $i 0
				if { $elect_pipe($i) != "INVALID" } {
					close_election $elect_pipe($i)
				}
				incr elect_serial
				set pfx CHILD$i.$elect_serial
				set elect_pipe($i) [start_election $pfx $qdir \
			    	    $env_cmd($i) [expr $nclients + 1] 0 \
				    $elect_timeout]
				set got_hold_elect($i) 1
			}
			if { $nm != 0 } {
				error_check_good newmaster_is_master $nm 1
				set got_master $nm
			}
			if { $nm2 != 0 } {
				error_check_good newmaster_is_master $nm2 1
				set got_master $nm2
			}
		}

		if { $nproced == 0 } {
			break
		}
	}
	error_check_good got_master $got_master 1
	cleanup_elections

	# We need multiple clients to proceed from here.
	if { $nclients < 2 } {
		puts "\tRep$tnum: Skipping for less than two clients."
		error_check_good masterenv_close [$masterenv close] 0
		for { set i 0 } { $i < $nclients } { incr i } {
			error_check_good clientenv_close($i) \
			    [$clientenv($i) close] 0
		}
		return
	}

	# Make sure all the clients are synced up and ready to be good
	# voting citizens.
	error_check_good master_flush [$masterenv rep_flush] 0
	while { 1 } {
		set nproced 0
		incr nproced [replprocessqueue $masterenv 1 0]
		for { set i 0 } { $i < $nclients } { incr i } {
			incr nproced [replprocessqueue $clientenv($i) \
			    [expr $i + 2] 0]
		}

		if { $nproced == 0 } {
			break
		}
	}

	# Now hold another election in the first client, this time with
	# a dead master.
	puts "\tRep$tnum.e: Starting election with dead master."
	error_check_good masterenv_close [$masterenv close] 0

	for { set i 0 } { $i < $nclients } { incr i } {
		replclear [expr $i + 2]
	}

	incr elect_serial
	set elect_pipe(0) [start_election C0 \
	    $qdir $env_cmd(0) $nclients 20 $elect_timeout]

	tclsleep 2

	# Process messages, and verify that the client with the highest
	# priority--client #1--wins.
	set got_newmaster 0
	set tries 10
	while { 1 } {
		set nproced 0
		set he 0
		set nm 0

		for { set i 0 } { $i < $nclients } { incr i } {
			set he 0
			set envid [expr $i + 2]
			set child_done [check_election $elect_pipe($i) nm2]
			if { $got_newmaster == 0 && $nm2 != 0} {
				error_check_good newmaster_is_master $nm2 \
				    [expr 1 + 2]
				set got_newmaster $nm2
				if { $nm2 == $envid } {
					error_check_good make_master($i) \
					    [$clientenv($i) rep_start -master] \
					    0
				}
			}
			incr nproced \
			    [replprocessqueue $clientenv($i) $envid 0 he nm]
			if { $he == 1 } {

				# Client #1 has priority 100;  everyone else
				# has priority 10.
				if { $i == 1 } {
					set pri 100
				} else {
					set pri 10
				}
				# error_check_bad client(0)_in_elect $i 0
				incr elect_serial
				set pfx CHILD$i.$elect_serial
				set elect_pipe($i) [start_election $pfx \
				    $qdir $env_cmd($i) $nclients \
				    $pri $elect_timeout]
				set got_hold_elect($i) 1
			}
			if { $nm != 0 } {
				error_check_good newmaster_is_master $nm \
				    [expr 1 + 2]
				set got_newmaster $nm

				# If this env is the new master, it needs to
				# configure itself as such--this is a different
				# env handle from the one that performed the
				# election.
				if { $nm == $envid } {
					error_check_good make_master($i) \
					    [$clientenv($i) rep_start -master] \
					    0
				}
			}
		}

		# We need to wait around to make doubly sure that the
		# election has finished...
		if { $nproced == 0 } {
			incr tries -1
			if { $tries == 0 } {
				break
			} else {
				tclsleep 1
			}
		} else {
			set tries 10
		}
	}

	# Verify that client #1 is actually the winner.
	error_check_good "client 1 wins" $got_newmaster [expr 1 + 2]

	cleanup_elections

	for { set i 0 } { $i < $nclients } { incr i } {
		error_check_good clientenv_close($i) [$clientenv($i) close] 0
	}

	replclose $testdir/MSGQUEUEDIR
}
