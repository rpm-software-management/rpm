# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep035script.tcl,v 12.4 2006/08/24 14:46:37 bostic Exp $
#
# Rep035 script - continually calls lock_detect, txn_checkpoint,
# or mpool_trickle.
#
# Usage: repscript clientdir apicall
# clientdir: client env directory
# apicall: detect, checkpoint, or trickle.
source ./include.tcl
source $test_path/test.tcl
source $test_path/testutils.tcl
source $test_path/reputils.tcl

set usage "repscript clientdir apicall"

# Verify usage
if { $argc != 2 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set clientdir [ lindex $argv 0 ]
set apicall [ lindex $argv 1 ]

set is_repchild 1
# Join the client env.
set envid 3
set cl2_cmd "berkdb_env_noerr -home $clientdir \
	-errfile /dev/stderr -errpfx CLIENT.$apicall \
	-txn -rep_client -rep_transport \[list $envid replsend\]"
# set cl2_cmd "berkdb_env_noerr -home $clientdir \
#	-errfile /dev/stderr -errpfx CLIENT.$apicall \
# 	-verbose {rep on} \
# 	-txn -rep_client -rep_transport \[list $envid replsend\]"
set clientenv [eval $cl2_cmd]
error_check_good script_c2env_open [is_valid_env $clientenv] TRUE

# Run chosen call continuously until the parent script creates
# a marker file to indicate completion.
switch -exact -- $apicall {
	archive {
		while { [file exists $testdir/marker.db] == 0 } {
			$clientenv log_archive -arch_remove
#			tclsleep 1
		}
	}
	detect {
		while { [file exists $testdir/marker.db] == 0 } {
			$clientenv lock_detect default
#			tclsleep 1
		}
	}
	checkpoint {
		while { [file exists $testdir/marker.db] == 0 } {
			$clientenv txn_checkpoint -force
			tclsleep 1
		}
	}
	trickle {
		while { [file exists $testdir/marker.db] == 0 } {
			$clientenv mpool_trickle 90
#			tclsleep 1
		}
	}
	default {
		puts "FAIL: unrecognized API call $apicall
	}
}

error_check_good clientenv_close [$clientenv close] 0

