# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: sdb013.tcl,v 1.3 2004/09/22 18:01:06 bostic Exp $
#
# TEST	sdb013
# TEST	Tests in-memory subdatabases.
# TEST	Create an in-memory subdb.  Test for persistence after
# TEST	overflowing the cache.  Test for conflicts when we have
# TEST	two in-memory files.

proc sdb013 { method { nentries 10 } args } {
	source ./include.tcl

	set tnum "013"
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb$tnum: skipping for method $method"
		return
	}
	puts "Subdb$tnum: $method ($args) in-memory subdb tests"

	# If we are using an env, then skip this test.  It needs its own.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		set env NULL
		incr eindex
		set env [lindex $args $eindex]
		puts "Subdb013 skipping for env $env"
		return
	}

	# Create the env, with a very small cache that we can easily
	# fill.
	env_cleanup $testdir
	set csize {0 32768 1}
	set env [berkdb_env -create -cachesize $csize -home $testdir -txn]
	error_check_good dbenv [is_valid_env $env] TRUE

	# Set filename to NULL; this causes the creation of an in-memory
	# subdb.
	set testfile ""
	set subdb subdb0

	puts "\tSubdb$tnum.a: Create in-mem subdb, add data, close."
	set sdb [eval {berkdb_open -create -mode 0644} \
	    $args -env $env {$omethod $testfile $subdb}]
	error_check_good dbopen [is_valid_db $sdb] TRUE

	sdb013_populate $sdb $method $nentries
	error_check_good sdb_close [$sdb close] 0

	# Do a bunch of writing to evict all pages from the memory pool.
	puts "\tSubdb$tnum.b: Create another db, overflow the cache."
	set dummyfile foo.db
	set db [eval {berkdb_open -create -mode 0644} $args -env $env\
	    $omethod $dummyfile]
	error_check_good dummy_open [is_valid_db $db] TRUE

	set entries [expr $nentries * 100]
	sdb013_populate $db $method $entries
	error_check_good dummy_close [$db close] 0

	# Make sure we can still open the in-memory subdb.
	puts "\tSubdb$tnum.c: Check we can still open the in-mem subdb."
	set sdb [eval {berkdb_open} \
	    $args -env $env {$omethod $testfile $subdb}]
	error_check_good sdb_reopen [is_valid_db $sdb] TRUE
	error_check_good sdb_close [$sdb close] 0

	puts "\tSubdb$tnum.d: Remove in-mem subdb."
	error_check_good \
	    sdb_remove [berkdb dbremove -env $env $testfile $subdb] 0

	puts "\tSubdb$tnum.e: Check we cannot open the in-mem subdb."
	set ret [catch {eval {berkdb_open_noerr} -env $env $args \
	    {$omethod $testfile $subdb}} db]
	error_check_bad dbopen $ret 0

	# Create two in-memory subdb and test for conflicts.  Try all the
	# combinations of named (NULL/NAME) and purely temporary
	# (NULL/NULL) databases.
	#
	foreach s1 { S1 "" } {
		foreach s2 { S2 "" } {
			puts "\tSubdb$tnum.f:\
			    2 in-memory subdbs (NULL/$s1, NULL/$s2)."
			set sdb1 [eval {berkdb_open -create -mode 0644} \
			    $args -env $env {$omethod $testfile $s1}]
puts "sdb1 open"
			error_check_good sdb1_open [is_valid_db $sdb1] TRUE
puts "open sdb2 with testfile $testfile s2 $s2"
			set sdb2 [eval {berkdb_open -create -mode 0644} \
			    $args -env $env {$omethod $testfile $s2}]
puts "sdb2 open"
			error_check_good sdb1_open [is_valid_db $sdb2] TRUE

			# Subdatabases are open, now put something in.
			set string1 STRING1
			set string2 STRING2
puts "populate"
			for { set i 1 } { $i <= $nentries } { incr i } {
				set key $i
				error_check_good sdb1_put \
				    [$sdb1 put $key $string1.$key] 0
				error_check_good sdb2_put \
				    [$sdb2 put $key $string2.$key] 0
			}
puts "check contents"
			# If the subs are both NULL/NULL, we have two handles
			# on the same db.  Skip testing the contents.
			if { $s1 != "" || $s2 != "" } {
				# This can't work when both subs are NULL/NULL.
				# Check contents.
				for { set i 1 } { $i <= $nentries } { incr i } {
					set key $i
					set ret1 [lindex \
					    [lindex [$sdb1 get $key] 0] 1]
					error_check_good \
					    sdb1_get $ret1 $string1.$key
					set ret2 [lindex \
					    [lindex [$sdb2 get $key] 0] 1]
					error_check_good \
					    sdb2_get $ret2 $string2.$key
				}
puts "close sdb1"
				error_check_good sdb1_close [$sdb1 close] 0
puts "close sdb2"
				error_check_good sdb2_close [$sdb2 close] 0

				# Reopen, make sure we get the right data.
				set sdb1 [eval {berkdb_open -mode 0644} \
				    $args -env $env {$omethod $testfile $s1}]
				error_check_good \
				    sdb1_open [is_valid_db $sdb1] TRUE
				set sdb2 [eval {berkdb_open -mode 0644} \
				    $args -env $env {$omethod $testfile $s2}]
				error_check_good \
				    sdb1_open [is_valid_db $sdb2] TRUE

				for { set i 1 } { $i <= $nentries } { incr i } {
					set key $i
					set ret1 [lindex \
					    [lindex [$sdb1 get $key] 0] 1]
					error_check_good \
					    sdb1_get $ret1 $string1.$key
					set ret2 [lindex \
					    [lindex [$sdb2 get $key] 0] 1]
					error_check_good \
					    sdb2_get $ret2 $string2.$key
				}
			}
			error_check_good sdb1_close [$sdb1 close] 0
			error_check_good sdb2_close [$sdb2 close] 0
		}
	}
	error_check_good env_close [$env close] 0
}

proc sdb013_populate { db method nentries } {
	source ./include.tcl

	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		set ret [eval \
		    {$db put $key [chop_data $method $str]}]
		error_check_good put $ret 0

		set ret [eval {$db get $key}]
		error_check_good \
		    get $ret [list [list $key [pad_data $method $str]]]
		incr count
	}
	close $did
}
