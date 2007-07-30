# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996,2007 Oracle.  All rights reserved.
#
# $Id: log006.tcl,v 12.5 2007/05/17 15:15:55 bostic Exp $
#
# TEST	log006
# TEST	Test log file auto-remove.
# TEST		Test normal operation.
# TEST		Test a long-lived txn.
# TEST		Test log_archive flags.
# TEST		Test db_archive flags.
# TEST		Test turning on later.
# TEST		Test setting via DB_CONFIG.
proc log006 { } {
	source ./include.tcl

	puts "Log006: Check auto-remove of log files."
	env_cleanup $testdir

	# Open the environment, set auto-remove flag.  Use smaller log
	# files to make more of them.
	puts "\tLog006.a: open environment, populate database."
	set lbuf 16384
	set lmax 65536
	set env [berkdb_env_noerr -log_remove \
	    -create -home $testdir -log_buffer $lbuf -log_max $lmax -txn]
	error_check_good envopen [is_valid_env $env] TRUE

	log006_put $testdir $env

	#
	# Check log files.  Using the small log file size, we should
	# have made a lot of log files, check that we have a reasonable
	# number left, less than 25.
	#
	set log_expect 25
	puts "\tLog006.b: Check log files removed."
	set lfiles [glob -nocomplain $testdir/log.*]
	set remlen [llength $lfiles]
	error_check_good lfiles_len [expr $remlen < $log_expect] 1
	error_check_good lfiles [lsearch $lfiles $testdir/log.0000000001] -1
	# Save last log file for later check.
	# Files may not be sorted, sort them and then save the last filename.
	set oldfile [lindex [lsort -ascii $lfiles] end]

	# Rerun log006_put with a long lived txn.
	#
	puts "\tLog006.c: Rerun put loop with long-lived transaction."
	cleanup $testdir $env
	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE

	# Give the txn something to do so no files can be removed.
	set testfile temp.db
	set db [eval {berkdb_open_noerr -create -mode 0644} \
	    -env $env -txn $txn -pagesize 8192 -btree $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	log006_put $testdir $env

	puts "\tLog006.d: Check log files not removed."
	set lfiles [glob -nocomplain $testdir/log.*]
	error_check_good lfiles2_len [expr [llength $lfiles] > $remlen] 1
	set lfiles [lsort -ascii $lfiles]
	error_check_good lfiles_chk [lsearch $lfiles $oldfile] 0
	error_check_good txn_commit [$txn commit] 0
	error_check_good db_close [$db close] 0
	error_check_good ckp1 [$env txn_checkpoint] 0
	error_check_good ckp2 [$env txn_checkpoint] 0

	puts "\tLog006.e: Run log_archive with -auto_remove flag."
	# When we're done, only the last log file should remain.
	set lfiles [glob -nocomplain $testdir/log.*]
	set oldfile [lindex [lsort -ascii $lfiles] end]

	# First, though, verify mutual-exclusiveness of flag.
	foreach f {-arch_abs -arch_data -arch_log} {
		set stat [catch {eval $env log_archive -arch_remove $f} ret]
		error_check_good stat $stat 1
		error_check_good la:$f:fail [is_substr $ret "illegal flag"] 1
	}
	# Now run it for real.
	set stat [catch {$env log_archive -arch_remove} ret]
	error_check_good stat $stat 0

	puts "\tLog006.f: Check only $oldfile remains."
	set lfiles [glob -nocomplain $testdir/log.*]
	error_check_good 1log [llength $lfiles] 1
	error_check_good lfiles_chk [lsearch $lfiles $oldfile] 0

	puts "\tLog006.g: Rerun put loop with long-lived transaction."
	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE
	log006_put $testdir $env
	error_check_good txn_commit [$txn commit] 0
	error_check_good ckp1 [$env txn_checkpoint] 0
	error_check_good ckp2 [$env txn_checkpoint] 0
	error_check_good env_close [$env close] 0

	#
	# Test db_archive's auto-remove flag.
	# After we are done, only the last log file should be there.
	# First check that the delete flag cannot be used with any
	# of the other flags.
	#
	puts "\tLog006.h: Run db_archive with delete flag."
	set lfiles [glob -nocomplain $testdir/log.*]
	set oldfile [lindex [lsort -ascii $lfiles] end]
	#
	# Again, first check illegal flag combinations with db_archive.
	#
	foreach f {-a -l -s} {
		set stat [catch {exec $util_path/db_archive $f -d -h $testdir} \
		    ret]
		error_check_good stat $stat 1
		error_check_good la:fail [is_substr $ret "illegal flag"] 1
	}
	set stat [catch {exec $util_path/db_archive -d -h $testdir} ret]
	error_check_good stat $stat 0

	puts "\tLog006.i: Check only $oldfile remains."
	set lfiles [glob -nocomplain $testdir/log.*]
	error_check_good 1log [llength $lfiles] 1
	error_check_good lfiles_chk [lsearch $lfiles $oldfile] 0

	#
	# Now rerun some parts with other env settings tested.
	#
	env_cleanup $testdir

	# First test that the option can be turned on later.
	# 1. Open env w/o auto-remove.
	# 2. Run log006_put.
	# 3. Verify log files all there.
	# 4. Call env set_flags to turn it on.
	# 5. Run log006_put.
	# 6. Verify log files removed.
	puts "\tLog006.j: open environment w/o auto remove, populate database."
	set env [berkdb_env -recover \
	    -create -home $testdir -log_buffer $lbuf -log_max $lmax -txn]
	error_check_good envopen [is_valid_env $env] TRUE

	log006_put $testdir $env

	puts "\tLog006.k: Check log files not removed."
	set lfiles [glob -nocomplain $testdir/log.*]
	error_check_good lfiles2_len [expr [llength $lfiles] > $remlen] 1
	set lfiles [lsort -ascii $lfiles]
	error_check_good lfiles [lsearch $lfiles $testdir/log.0000000001] 0

	puts "\tLog006.l: turn on auto remove and repopulate database."
	error_check_good sf [$env set_flags -log_remove on] 0

	log006_put $testdir $env

	puts "\tLog006.m: Check log files removed."
	set lfiles [glob -nocomplain $testdir/log.*]
	error_check_good lfiles_len [expr [llength $lfiles] < $log_expect] 1
	error_check_good lfiles [lsearch $lfiles $testdir/log.0000000001] -1
	error_check_good env_close [$env close] 0

	#
	# Configure via DB_CONFIG.
	#
	env_cleanup $testdir

	puts "\tLog006.n: Test setting via DB_CONFIG."
	# Open the environment, w/o remove flag, but DB_CONFIG.
	set cid [open $testdir/DB_CONFIG w]
	puts $cid "set_flags db_log_autoremove"
	close $cid
	set env [berkdb_env -recover \
	    -create -home $testdir -log_buffer $lbuf -log_max $lmax -txn]
	error_check_good envopen [is_valid_env $env] TRUE

	log006_put $testdir $env

	puts "\tLog006.o: Check log files removed."
	set lfiles [glob -nocomplain $testdir/log.*]
	error_check_good lfiles_len [expr [llength $lfiles] < $log_expect] 1
	error_check_good lfiles [lsearch $lfiles $testdir/log.0000000001] -1
	error_check_good env_close [$env close] 0

}

#
# Modified from test003.
#
proc log006_put { testdir env } {
	set testfile log006.db
	#
	# Specify a pagesize so we can control how many log files
	# are created and left over.
	#
	set db [eval {berkdb_open_noerr -create -mode 0644} \
	    -env $env -auto_commit -pagesize 8192 -btree $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	set lmax [$env get_lg_max]
	set file_list [get_file_list]
	set count 0
	foreach f $file_list {
		if { [string compare [file type $f] "file"] != 0 } {
			continue
		}
		set key $f
		# Should really catch errors
		set fid [open $f r]
		fconfigure $fid -translation binary
		# Read in less than the maximum log size.
		set data [read $fid [expr $lmax - [expr $lmax / 8]]]
		close $fid

		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
		set ret [eval {$db put} $txn {$key $data}]
		error_check_good put $ret 0
		error_check_good txn [$t commit] 0
		if { $count % 10 == 0 } {
			error_check_good ckp($count) [$env txn_checkpoint] 0
		}

		incr count
	}
	error_check_good db_close [$db close] 0
}
