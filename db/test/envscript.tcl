# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004,2007 Oracle.  All rights reserved.
#
# $Id: envscript.tcl,v 12.7 2007/05/17 15:15:55 bostic Exp $
#
# Envscript -- for use with env012, DB_REGISTER test.
# Usage: envscript testdir testfile putget key data recover envclose wait
# testdir: directory containing the env we are joining.
# testfile: file name for database.
# putget: What to do in the db: put, get, or loop.
# key: key to store or get
# data: data to store or get
# recover: include or omit the -recover flag in opening the env.
# envclose: close env at end of test?
# wait: how many seconds to wait before closing env at end of test.

source ./include.tcl

set usage "envscript testdir testfile putget key data recover envclose wait"

# Verify usage
if { $argc != 7 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set testdir [ lindex $argv 0 ]
set testfile [ lindex $argv 1 ]
set putget [lindex $argv 2 ]
set key [ lindex $argv 3 ]
set data [ lindex $argv 4 ]
set recover [ lindex $argv 5 ]
set wait [ lindex $argv 6 ]

set flags {}
if { $recover == "RECOVER" } {
	set flags " -recover "
}

# Open and register environment.
if {[catch {eval {berkdb_env} \
    -create -home $testdir -txn -register $flags} dbenv]} {
    	puts "FAIL: opening env returned $dbenv"
}
error_check_good envopen [is_valid_env $dbenv] TRUE

# Open database, put or get, close database.
if {[catch {eval {berkdb_open} \
    -create -auto_commit -btree -env $dbenv $testfile} db]} {
	puts "FAIL: opening db returned $db"
}
error_check_good dbopen [is_valid_db $db] TRUE

switch $putget {
	PUT {
		set txn [$dbenv txn]
		error_check_good db_put [eval {$db put} -txn $txn $key $data] 0
		error_check_good txn_commit [$txn commit] 0
	}
	GET {
		set ret [$db get $key]
		error_check_good db_get [lindex [lindex $ret 0] 1] $data
	}
	LOOP {
		while { 1 } {
			set txn [$dbenv txn]
			error_check_good db_put \
			    [eval {$db put} -txn $txn $key $data] 0
			error_check_good txn_commit [$txn commit] 0
			tclsleep 1
		}
	}
	default {
		puts "FAIL: Unrecognized putget value $putget"
	}
}

error_check_good db_close [$db close] 0

# Wait.
while { $wait > 0 } {
puts "waiting ... wait is $wait"
	tclsleep 1
	incr wait -1
}

error_check_good env_close [$dbenv close] 0
