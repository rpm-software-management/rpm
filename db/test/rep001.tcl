# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001
#	Sleepycat Software.  All rights reserved.
#
# Id: rep001.tcl,v 11.2 2001/10/05 02:38:09 bostic Exp 
#
# TEST  rep001
# TEST	Replication smoke test.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  verify that the database on the client is correct.


proc rep001 { method args } {
	source ./include.tcl
	global testdir

	env_cleanup $testdir

	replsetup $testdir/REPDIR_MSGQUEUE

	set masterdir $testdir/REPDIR_MASTER
	set clientdir $testdir/REPDIR_CLIENT

	file mkdir $masterdir
	file mkdir $clientdir

	puts "Rep001: Replication sanity test."

	# Open a master.
	repladd 1
	set masterenv [berkdb env -create \
	    -home $masterdir -txn -rep_master -rep_transport [list 1 replsend]]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set clientenv [berkdb env -create \
	    -home $clientdir -txn -rep_client -rep_transport [list 2 replsend]]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Run a modified test001 in the master.
	puts "\tRep001.a: Running test001 in replicated env."
	eval rep_test001 $method 1000 "01" -env $masterenv $args

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Verify the database in the client dir.
	puts "\tRep001.b: Verifying client database contents."
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test001.db $clientenv "" $testdir/t1 test001.check \
	    dump_file_direction "-first" "-next"
	filesort $t1 $t3
	error_check_good diff_files($t2,$t3) [filecmp $t2 $t3] 0

	verify_dir $clientdir "\tRep001.c:" 0 0 1
}

proc rep_test001 { method {nentries 10000} {tnum "01"} args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "\tRep0$tnum: $method ($args) $nentries equal key/data pairs"

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	# If we are not using an external env, then test setting
	# the database cache size and using multiple caches.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		append args " -cachesize {0 1048576 3} "
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set db [eval {berkdb_open \
	     -create -mode 0644} $args $omethod $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""

	if { [is_record_based $method] == 1 } {
		set checkfunc test001_recno.check
		append gflags " -recno"
	} else {
		set checkfunc test001.check
	}
	puts "\t\tRep0$tnum.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			global kvals

			set key [expr $count + 1]
			if { 0xffffffff > 0 && $key > 0xffffffff } {
				set key [expr $key - 0x100000000]
			}
			if { $key == 0 || $key - 0xffffffff == 1 } {
				incr key
				incr count
			}
			set kvals($key) [pad_data $method $str]
		} else {
			set key $str
			set str [reverse $str]
		}
		set curtxn [$env txn]
		set ret [eval {$db put} \
		    -txn $curtxn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0
		error_check_good txn_commit($key) [$curtxn commit] 0

		set ret [eval {$db get} $gflags {$key}]
		error_check_good \
		    get $ret [list [list $key [pad_data $method $str]]]

		# Test DB_GET_BOTH for success
		set ret [$db get -get_both $key [pad_data $method $str]]
		error_check_good \
		    getboth $ret [list [list $key [pad_data $method $str]]]

		# Test DB_GET_BOTH for failure
		set ret [$db get -get_both $key [pad_data $method BAD$str]]
		error_check_good getbothBAD [llength $ret] 0

		incr count
	}
	close $did
	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\t\tRep0$tnum.b: dump file"
	dump_file $db $txn $t1 $checkfunc
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary (or ints)
	if { [is_record_based $method] == 1 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $nentries} {incr i} {
			set j [expr $i]
			if { 0xffffffff > 0 && $j > 0xffffffff } {
				set j [expr $j - 0x100000000]
			}
			if { $j == 0 } {
				incr i
				incr j
			}
			puts $oid $j
		}
		close $oid
	} else {
		set q q
		filehead $nentries $dict $t2
	}
	filesort $t2 $t3
	file rename -force $t3 $t2
	filesort $t1 $t3

	error_check_good \tRep0$tnum:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0

	puts "\t\tRep0$tnum.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile $env $txn $t1 $checkfunc \
	    dump_file_direction "-first" "-next"
	if { [string compare $omethod "-recno"] != 0 } {
		filesort $t1 $t3
	}

	error_check_good \tRep0$tnum:diff($t2,$t3) \
	    [filecmp $t2 $t3] 0

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\t\tRep0$tnum.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile $env $txn $t1 $checkfunc \
	    dump_file_direction "-last" "-prev"

	if { [string compare $omethod "-recno"] != 0 } {
		filesort $t1 $t3
	}

	error_check_good \tRep0$tnum:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0
}

