# See the file LICENSE for redistribution information.
#
# Copyright (c) 2002-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep005.tcl,v 11.29 2003/10/27 15:19:18 sandstro Exp $
#
# TEST  rep005
# TEST	Replication election test with error handling.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  hold an election among a group of clients to make sure they select
# TEST  a proper master from amongst themselves, forcing errors at various
# TEST	locations in the election path.

proc rep005 { method args } {
	source ./include.tcl

	if { [is_btree $method] == 0 } {
		puts "Rep005: Skipping for method $method."
		return
	}
	global rand_init
	error_check_good set_random_seed [berkdb srand $rand_init] 0

	set tnum "005"
	set niter 10
	set nclients 3
	env_cleanup $testdir

	set qdir $testdir/MSGQUEUEDIR
	replsetup $qdir

	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir

	for { set i 0 } { $i < $nclients } { incr i } {
		set clientdir($i) $testdir/CLIENTDIR.$i
		file mkdir $clientdir($i)
	}

	puts -nonewline \
	    "Rep$tnum: Replication election error test with $nclients clients."
	puts -nonewline \
	    " Started at: "
	puts [clock format [clock seconds] -format "%H:%M %D"]

	# Open a master.
	repladd 1
	set env_cmd(M) "berkdb_env -create -log_max 1000000 -home $masterdir \
	    -txn nosync -rep_master -rep_transport \[list 1 replsend\]"
# To debug elections, uncomment the line below and further below
# for the clients to turn on verbose.  Also edit reputils.tcl
# in proc start_election and swap the 2 commented lines with
# their counterpart.
#	set env_cmd(M) "berkdb_env_noerr -create -log_max 1000000 \
#	    -home $masterdir -txn nosync -rep_master \
#	    -verbose {rep on} -errpfx MASTER -errfile /dev/stderr \
#	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M)]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open the clients.
	for { set i 0 } { $i < $nclients } { incr i } {
		set envid [expr $i + 2]
		repladd $envid
		set env_cmd($i) "berkdb_env -create -home $clientdir($i) \
		    -txn nosync -rep_client \
		    -rep_transport \[list $envid replsend\]"
#		set env_cmd($i) "berkdb_env_noerr -create -home $clientdir($i) \
#		    -verbose {rep on} -errpfx CLIENT$i -errfile /dev/stderr \
#		    -txn nosync -rep_client \
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

	error_check_good masterenv_close [$masterenv close] 0

	for { set i 0 } { $i < $nclients } { incr i } {
		replclear [expr $i + 2]
	}
	#
	# We set up the error list for each client.  We know that the
	# first client is the one calling the election, therefore, add
	# the error location on sending the message (electsend) for that one.
	set m "Rep$tnum"
	set count 0
	set win -1
	#
	# A full test can take a long time to run.  For normal testing
	# pare it down a lot so that it runs in a shorter time.
	#
	set c0err { none electinit none none }
	set c1err $c0err
	set c2err $c0err
	set numtests [expr [llength $c0err] * [llength $c1err] * \
	    [llength $c2err]]
	puts "\t$m.d: Starting $numtests election with error tests"
	set last_win -1
	set win -1
	foreach c0 $c0err {
		foreach c1 $c1err {
			foreach c2 $c2err {
				set elist [list $c0 $c1 $c2]
				rep005_elect env_cmd clientenv $qdir $m \
				    $count win last_win $elist
				incr count
			}
		}
	}

	for { set i 0 } { $i < $nclients } { incr i } {
		error_check_good clientenv_close($i) [$clientenv($i) close] 0
	}

	replclose $testdir/MSGQUEUEDIR
	puts -nonewline \
	    "Rep$tnum: Completed at: "
	puts [clock format [clock seconds] -format "%H:%M %D"]
}

proc rep005_elect { ecmd cenv qdir msg count winner lsn_lose elist } {
	global elect_timeout elect_serial
	global is_windows_test
	upvar $ecmd env_cmd
	upvar $cenv clientenv
	upvar $winner win
	upvar $lsn_lose last_win

	set elect_timeout 5000000
	set nclients [llength $elist]
	set nsites [expr $nclients + 1]

	set cl_list {}
	for { set i 0 } { $i < $nclients } { incr i } {
		set err_cmd($i) [lindex $elist $i]
		set elect_pipe($i) INVALID
		replclear [expr $i + 2]
		lappend cl_list $i
	}

	# Select winner.  We want to test biggest LSN wins, and secondarily
	# highest priority wins.  If we already have a master, make sure
	# we don't start a client in that master.
	set el 0
	if { $win == -1 } {
		if { $last_win != -1 } {
			set cl_list [lreplace $cl_list $last_win $last_win]
			set el $last_win
		}
		set windex [berkdb random_int 1 [expr [llength $cl_list] - 1]]
		set win [lindex $cl_list $windex]
	} else {
		# Easy case, if we have a master, the winner must be the
		# same one as last time, just use $win.
		# If client0 is the current existing master, start the
		# election in client 1.
		if {$win == 0} {
			set el 1
		}
	}
	# Winner has priority 100.  If we are testing LSN winning, the
	# make sure the lowest LSN client has the highest priority.
	# Everyone else has priority 10.
	for { set i 0 } { $i < $nclients } { incr i } {
		if { $i == $win } {
			set pri($i) 100
		} elseif { $i == $last_win } {
			set pri($i) 200
		} else {
			set pri($i) 10
		}
	}

	puts "\t$msg.d.$count: Start election (win=client$win) $elist"
	incr elect_serial
	set pfx "CHILD$el.$elect_serial"
	# Windows requires a longer timeout.
	if { $is_windows_test == 1 } {
		set elect_timeout [expr $elect_timeout * 3]
	}
	set elect_pipe($el) [start_election $pfx $qdir $env_cmd($el) \
	    $nsites $pri($el) $elect_timeout $err_cmd($el)]

	tclsleep 2

	set got_newmaster 0
	set tries 10
	while { 1 } {
		set nproced 0
		set he 0
		set nm 0
		set nm2 0

		for { set i 0 } { $i < $nclients } { incr i } {
			set he 0
			set envid [expr $i + 2]
			set child_done [check_election $elect_pipe($i) nm2]
			if { $got_newmaster == 0 && $nm2 != 0 } {
				error_check_good newmaster_is_master $nm2 \
				    [expr $win + 2]
				set got_newmaster $nm2

				# If this env is the new master, it needs to
				# configure itself as such--this is a different
				# env handle from the one that performed the
				# election.
				if { $nm2 == $envid } {
					error_check_good make_master($i) \
					    [$clientenv($i) rep_start -master] \
					    0
				}
			}
			incr nproced \
			    [replprocessqueue $clientenv($i) $envid 0 he nm]
# puts "Tries $tries: Processed queue for client $i, $nproced msgs he $he nm $nm nm2 $nm2"
			if { $he == 1 } {
				#
				# Only close down the election pipe if the
				# previously created one is done and
				# waiting for new commands, otherwise
				# if we try to close it while it's in
				# progress we hang this main tclsh.
				#
				if { $elect_pipe($i) != "INVALID" && \
				    $child_done == 1 } {
					close_election $elect_pipe($i)
					set elect_pipe($i) "INVALID"
				}
# puts "Starting election on client $i"
				if { $elect_pipe($i) == "INVALID" } {
					incr elect_serial
					set pfx "CHILD$i.$elect_serial"
					set elect_pipe($i) [start_election \
					    $pfx $qdir \
					    $env_cmd($i) $nsites $pri($i) \
					    $elect_timeout $err_cmd($i)]
					set got_hold_elect($i) 1
				}
			}
			if { $nm != 0 } {
				error_check_good newmaster_is_master $nm \
				    [expr $win + 2]
				set got_newmaster $nm

				# If this env is the new master, it needs to
				# configure itself as such--this is a different
				# env handle from the one that performed the
				# election.
				if { $nm == $envid } {
					error_check_good make_master($i) \
					    [$clientenv($i) rep_start -master] \
					    0
					if { [expr $count % 10] == 0 } {
						set dbname rep005.$count.db
						set db [berkdb_open -env \
						    $clientenv($i) \
						    -auto_commit \
						    -create -btree $dbname]
						error_check_good dbopen \
						    [is_valid_db $db] TRUE
						error_check_good dbclose \
						    [$db close] 0
					}
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
	error_check_good "client $win wins" $got_newmaster [expr $win + 2]

	cleanup_elections

	#
	# Make sure that we've really processed all the post-election
	# sync-up messages.
	#
	while { 1 } {
		set nproced 0
		for { set i 0 } { $i < $nclients } { incr i } {
			incr nproced [replprocessqueue $clientenv($i) \
			    [expr $i + 2] 0]
		}
		if { $nproced == 0 } {
			break
		}
	}

	#
	# Sometimes test elections with an existing master.
	# Other times test elections without master by closing the
	# master we just elected and creating a new client.
	# We want to weight it to close the new master.  So, use
	# a list to cause closing about 70% of the time.
	#
	set close_list { 0 0 0 1 1 1 1 1 1 1}
	set close_len [expr [llength $close_list] - 1]
	set close_index [berkdb random_int 0 $close_len]
	if { [lindex $close_list $close_index] == 1 } {
		puts -nonewline "\t$msg.e.$count: Closing "
		error_check_good newmaster_close [$clientenv($win) close] 0
		#
		# If the next test should win via LSN then remove the
		# env before starting the new client so that we
		# can guarantee this client doesn't win the next one.
		set lsn_win { 0 0 0 0 1 1 1 1 1 1 }
		set lsn_len [expr [llength $lsn_win] - 1]
		set lsn_index [berkdb random_int 0 $lsn_len]
		if { [lindex $lsn_win $lsn_index] == 1 } {
			set last_win $win
			set dirindex [lsearch -exact $env_cmd($win) "-home"]
			incr dirindex
			set lsn_dir [lindex $env_cmd($win) $dirindex]
			env_cleanup $lsn_dir
			puts -nonewline "and cleaning "
		} else {
			set last_win -1
		}
		puts "new master, new client $win"
		set clientenv($win) [eval $env_cmd($win)]
		error_check_good cl($win) [is_valid_env $clientenv($win)] TRUE
		set win -1
		#
		# Since we started a new client we want to give them
		# all a chance to process everything outstanding before
		# the election on the next iteration.
		while { 1 } {
			set nproced 0
			for { set i 0 } { $i < $nclients } { incr i } {
				incr nproced [replprocessqueue $clientenv($i) \
				    [expr $i + 2] 0]
			}
			if { $nproced == 0 } {
				break
			}
		}
	}
}
