# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: fop003.tcl,v 1.4 2003/09/08 16:54:10 sandstro Exp $
#
# TEST	fop003
# TEST
# TEST	Test behavior of create and truncate for compatibility
# TEST	with sendmail.
# TEST	1.  DB_TRUNCATE is not allowed with locking or transactions.
# TEST	2.  Can -create into zero-length existing file.
# TEST	3.  Can -create into non-zero-length existing file if and
# TEST	only if DB_TRUNCATE is specified.
proc fop003 { } {
	global errorInfo
	source ./include.tcl
	env_cleanup $testdir

	set tnum "003"
	set testfile fop$tnum.db
	puts "Fop$tnum: Test of required behavior for sendmail."

	puts "\tFop$tnum.a: -truncate is not allowed within\
	    txn or locking env."
	set envflags "lock txn"
	foreach flag $envflags {
		set env [berkdb_env_noerr -create -home $testdir -$flag]
		set db [berkdb_open_noerr -create -btree -env $env $testfile]
		error_check_good db_open [is_valid_db $db] TRUE
		error_check_good db_close [$db close] 0
		catch {[berkdb_open_noerr -truncate -btree -env $env \
		    $testfile]} res
		error_check_good "$flag env not allowed" [is_substr $res \
		    "DB_TRUNCATE illegal with locking specified"] 1
		error_check_good dbremove [$env dbremove $testfile] 0
		error_check_good env_close [$env close] 0
		error_check_good envremove [berkdb envremove -home $testdir] 0
	}

	puts "\tFop$tnum.b: -create is allowed on open of existing\
	    zero-length file."
	# Create an empty file, then open with -create.  We get an
	# error message warning us that this does not look like a
	# DB file, but the open should succeed.
	set fd [open $testdir/foo w]
	close $fd
	catch {set db [berkdb_open_noerr -create -btree $testdir/foo]} res
	error_check_good open_fail [is_substr $errorInfo \
	    "unexpected file type or format"] 1
	error_check_good db_open [is_valid_db $db] TRUE
	error_check_good db_close [$db close] 0

	puts "\tFop$tnum.c: -create is ignored on open of existing\
	    non-zero-length file."
	# Create a db file.  Close and reopen with -create.  Make
	# sure that we still have the same file by checking the contents.
	set key "key"
	set data "data"
	set file "file.db"
	set db [berkdb_open -create -btree $testdir/$file]
	error_check_good db_open [is_valid_db $db] TRUE
	error_check_good db_put [$db put $key $data] 0
	error_check_good db_close [$db close] 0
	set db [berkdb_open -create -btree $testdir/$file]
	error_check_good db_open2 [is_valid_db $db] TRUE
	set ret [$db get $key]
	error_check_good db_get [lindex [lindex $ret 0] 1] $data
	error_check_good db_close2 [$db close] 0

	puts "\tFop$tnum.d: -create is allowed on open -truncate of\
	    existing non-zero-length file."
	# Use the file we already have with -truncate flag.  The open
	# should be successful, and when we query for the key that
	# used to be there, we should get nothing.
	set db [berkdb_open -create -truncate -btree $testdir/$file]
	error_check_good db_open3 [is_valid_db $db] TRUE
	set ret [$db get $key]
	error_check_good db_get [lindex [lindex $ret 0] 1] ""
	error_check_good db_close3 [$db close] 0

}
