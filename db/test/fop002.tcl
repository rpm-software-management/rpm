# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: fop002.tcl,v 1.5 2003/09/08 16:41:06 sandstro Exp $
#
# TEST	fop002.tcl
# TEST	Test file system operations in the presence of bad permissions.
proc fop002 { } {
	source ./include.tcl

	env_cleanup $testdir
	puts "\nFop002: File system ops and permissions. "
	if { $is_windows_test == 1 } {
		puts "\tSkipping permissions test for Windows platform."
		return
	}

	# Create database with -rw-r--r-- permissions.
	set perms "0644"
	set testfile $testdir/a.db
	set destfile $testdir/b.db

	set db [berkdb_open -create -btree -mode $perms $testfile]
	error_check_good db_open [is_valid_db $db] TRUE
	error_check_good db_put [$db put 1 a] 0
	error_check_good db_close [$db close] 0

	# Eliminate all read and write permission, and try to execute
	# file ops.  They should fail.
	set res [exec chmod 0000 $testfile]
	error_check_good remove_permissions [llength $res] 0
	# Put remove last on the list of ops since it should succeed
	# at the end of the test, removing the test file.
	set ops [list open_create open rename remove]
	set rdonly 0

	puts "\tFop002.a: Test with neither read nor write permission."
	foreach op $ops {
		puts "\t\tFop002.a: Testing $op for failure."
		switch $op {
			open {
				test_$op $testfile $rdonly 1
			}
			rename {
				test_$op $testfile $destfile 1
			}
			open_create -
			remove {
				test_$op $testfile 1
			}
		}
	}

	# Change permissions to read-only.
	puts "\tFop002.b: Test with read-only permission."
	set rdonly 1

	set res [exec chmod 0444 $testfile]
	error_check_good set_readonly [llength $res] 0

	foreach op $ops {
		puts "\t\tFop002.b: Testing $op for success."
		switch $op {
			open {
				test_$op $testfile $rdonly 0
			}
			rename {
				test_$op $testfile $destfile 0
				# Move it back so later tests work
				test_$op $destfile $testfile 0
			}
			open_create {
				puts "\t\tSkipping open_create with read-only."
			}
			remove {
				test_$op $testfile 0
			}
		}
	}
}

proc test_remove { testfile {expectfail 0} } {
	catch { berkdb dbremove $testfile } res
	if { $expectfail == 1 } {
		error_check_good remove_err $res "db remove:permission denied"
	} else {
		error_check_good remove $res 0
	}
}

proc test_rename { testfile destfile {expectfail 0} } {
	catch { berkdb dbrename $testfile $destfile } res
	if { $expectfail == 1 } {
		error_check_good rename_err $res "db rename:permission denied"
	} else {
		error_check_good rename $res 0
	}
}

proc test_open_create { testfile {expectfail 0} } {
	set stat [catch { set db \
	    [berkdb_open -create -btree $testfile]} res]
	if { $expectfail == 1 } {
		error_check_good open_create_err $res \
		    "db open:permission denied"
	} else {
		error_check_good open_create $stat 0
		# Since we succeeded, we have to close the db.
		error_check_good db_close [$db close] 0
	}
}

proc test_open { testfile {readonly 0} {expectfail 0} } {
	if { $readonly == 1 } {
		set stat [catch {set db \
		    [berkdb_open -rdonly -btree $testfile]} res]
	} else {
		set stat [catch {set db [berkdb_open -btree $testfile]} res]
	}
	if { $expectfail == 1 } {
		error_check_good open_err $res \
		    "db open:permission denied"
	} else {
		error_check_good db_open $stat 0
		error_check_good db_close [$db close] 0
	}
}

