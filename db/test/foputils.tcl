# See the file LICENSE for redistribution information.
#
# Copyright (c) 2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: foputils.tcl,v 11.3 2003/09/04 23:41:11 bostic Exp $
#
proc do_op {op args txn env} {
	switch -exact $op {
		rename { do_rename $args $txn $env }
		remove { do_remove $args $txn $env }
		open_create { do_create $args $txn $env }
		open { do_open $args $txn $env }
		open_excl { do_create_excl $args $txn $env }
		truncate { do_truncate $args $txn $env }
		default { puts "FAIL: operation $op not recognized" }
	}
}

proc do_rename {args txn env} {
	# Pull db names out of $args
	set oldname [lindex $args 0]
	set newname [lindex $args 1]

	if {[catch {eval $env dbrename -txn $txn \
	    $oldname.db $newname.db} result]} {
		return $result
	} else {
		return 0
	}
}

proc do_remove {args txn env} {
	if {[catch {eval $env dbremove -txn $txn $args.db} result]} {
		return $result
	} else {
		return 0
	}
}

proc do_create {args txn env} {
	if {[catch {eval berkdb_open -create -btree -env $env \
	    -txn $txn $args.db} result]} {
		return $result
	} else {
		return 0
	}
}

proc do_open {args txn env} {
	if {[catch {eval berkdb_open -btree -env $env \
	    -txn $txn $args.db} result]} {
		return $result
	} else {
		return 0
	}
}

proc do_create_excl {args txn env} {
	if {[catch {eval berkdb_open -create -excl -btree -env $env \
	    -txn $txn $args.db} result]} {
		return $result
	} else {
		return 0
	}
}

proc do_truncate {args txn env} {
	# First we have to get a handle.  We omit the -create flag
	# because testing of truncate is meaningful only in cases
	# where the database already exists.
	set db [berkdb_open -btree -env $env -txn $txn $args.db]
	error_check_good db_open [is_valid_db $db] TRUE

	if {[catch {$db truncate -txn $txn} result]} {
		return $result
	} else {
		return 0
	}
}

proc create_tests { op1 op2 exists noexist open retval { end1 "" } } {
	set retlist {}
	switch $op1 {
		rename {
			# Use first element from exists list
			set from [lindex $exists 0]
			# Use first element from noexist list
			set to [lindex $noexist 0]

			# This is the first operation, which should succeed
			set op1ret [list $op1 "$from $to" 0 $end1]

			# Adjust 'exists' and 'noexist' list if and only if
			# txn1 was not aborted.
			if { $end1 != "abort" } {
				set exists [lreplace $exists 0 0 $to]
				set noexist [lreplace $noexist 0 0 $from]
			}
		}
		remove {
			set from [lindex $exists 0]
			set op1ret [list $op1 $from 0 $end1]

			if { $end1 != "abort" } {
				set exists [lreplace $exists 0 0]
				set noexist [lreplace $noexist 0 0 $from]
			}
		}
		open_create -
		open -
		truncate {
			set from [lindex $exists 0]
			set op1ret [list $op1 $from 0 $end1]

			if { $end1 != "abort" } {
				set exists [lreplace $exists 0 0]
				set open [list $from]
			}

			# Eliminate the 1st element in noexist: it is
			# equivalent to the 2nd element (neither ever exists).
			set noexist [lreplace $noexist 0 0]
		}
		open_excl {
			# Use first element from noexist list
			set from [lindex $noexist 0]
			set op1ret [list $op1 $from 0 $end1]

			if { $end1 != "abort" } {
				set noexist [lreplace $noexist 0 0]
				set open [list $from]
			}

			# Eliminate the 1st element in exists: it is
			# equivalent to the 2nd element (both already exist).
			set exists [lreplace $exists 0 0]
		}
	}

	# Generate possible second operations given the return value.
	set op2list [create_op2 $op2 $exists $noexist $open $retval]

	foreach o $op2list {
		lappend retlist [list $op1ret $o]
	}
	return $retlist
}

proc create_badtests { op1 op2 exists noexist open retval {end1 ""} } {
	set retlist {}
	switch $op1 {
		rename {
			# Use first element from exists list
			set from [lindex $exists 0]
			# Use first element from noexist list
			set to [lindex $noexist 0]

			# This is the first operation, which should fail
			set op1list1 \
			    [list $op1 "$to $to" "no such file" $end1]
			set op1list2 \
			    [list $op1 "$to $from" "no such file" $end1]
			set op1list3 \
			    [list $op1 "$from $from" "file exists" $end1]
			set op1list [list $op1list1 $op1list2 $op1list3]

			# Generate second operations given the return value.
			set op2list [create_op2 \
			    $op2 $exists $noexist $open $retval]
			foreach op1 $op1list {
				foreach op2 $op2list {
					lappend retlist [list $op1 $op2]
				}
			}
			return $retlist
		}
		remove -
		open -
		truncate {
			set file [lindex $noexist 0]
			set op1list [list $op1 $file "no such file" $end1]

			set op2list [create_op2 \
			    $op2 $exists $noexist $open $retval]
			foreach op2 $op2list {
				lappend retlist [list $op1list $op2]
			}
			return $retlist
		}
		open_excl {
			set file [lindex $exists 0]
			set op1list [list $op1 $file "file exists" $end1]
			set op2list [create_op2 \
			    $op2 $exists $noexist $open $retval]
			foreach op2 $op2list {
				lappend retlist [list $op1list $op2]
			}
			return $retlist
		}
	}
}

proc create_op2 { op2 exists noexist open retval } {
	set retlist {}
	switch $op2 {
		rename {
			# Successful renames arise from renaming existing
			# to non-existing files.
			if { $retval == 0 } {
				set old $exists
				set new $noexist
				set retlist \
				    [build_retlist $op2 $old $new $retval]
			}
			# "File exists" errors arise from renaming existing
			# to existing files.
			if { $retval == "file exists" } {
				set old $exists
				set new $exists
				set retlist \
				    [build_retlist $op2 $old $new $retval]
			}
			# "No such file" errors arise from renaming files
			# that don't exist.
			if { $retval == "no such file" } {
				set old $noexist
				set new $exists
				set retlist1 \
				    [build_retlist $op2 $old $new $retval]

				set old $noexist
				set new $noexist
				set retlist2 \
				    [build_retlist $op2 $old $new $retval]

				set retlist [concat $retlist1 $retlist2]
			}
			# "File open" errors arise from trying to rename
			# open files.
			if { $retval == "file is open" } {
				set old $open
				set new $noexist
				set retlist \
				   [build_retlist $op2 $old $new $retval]
			}
		}
		remove {
			# Successful removes result from removing existing
			# files.
			if { $retval == 0 } {
				set file $exists
			}
			# "File exists" does not happen in remove.
			if { $retval == "file exists" } {
				return
			}
			# "No such file" errors arise from trying to remove
			# files that don't exist.
			if { $retval == "no such file" } {
				set file $noexist
			}
			# "File is open" errors arise from trying to remove
			# open files.
			if { $retval == "file is open" } {
				set file $open
			}
			set retlist [build_retlist $op2 $file "" $retval]
		}
		open_create {
			# Open_create should be successful with existing,
			# open, or non-existing files.
			if { $retval == 0 } {
				set file [concat $exists $open $noexist]
			}
			# "File exists", "file is open", and "no such file"
			# do not happen in open_create.
			if { $retval == "file exists" || \
			    $retval == "file is open" || \
			    $retval == "no such file" } {
				return
			}
			set retlist [build_retlist $op2 $file "" $retval]
		}
		open {
			# Open should be successful with existing or open files.
			if { $retval == 0 } {
				set file [concat $exists $open]
			}
			# "No such file" errors arise from trying to open
			# non-existent files.
			if { $retval == "no such file" } {
				set file $noexist
			}
			# "File exists" and "file is open" do not happen
			# in open.
			if { $retval == "file exists" || \
			    $retval == "file is open" } {
				return
			}
			set retlist [build_retlist $op2 $file "" $retval]
		}
		open_excl {
			# Open_excl should be successful with non-existent files.
			if { $retval == 0 } {
				set file $noexist
			}
			# "File exists" errors arise from trying to open
			# existing files.
			if { $retval == "file exists" } {
				set file [concat $exists $open]
			}
			# "No such file" and "file is open" do not happen
			# in open_excl.
			if { $retval == "no such file" || \
			    $retval == "file is open" } {
				return
			}
			set retlist [build_retlist $op2 $file "" $retval]
		}
		truncate {
			# Truncate should be successful with existing files.
			if { $retval == 0 } {
				set file $exists
			}
			# No other return values are meaningful to test since
			# do_truncate starts with an open and we've already
			# tested open.
			if { $retval == "file is open" || \
			    $retval == "file exists" || \
			    $retval == "no such file" } {
				return
			}
			set retlist [build_retlist $op2 $file "" $retval]
		}
	}
	return $retlist
}

proc build_retlist { op2 file1 file2 retval } {
	set retlist {}
	if { $file2 == "" } {
		foreach f1 $file1 {
			lappend retlist [list $op2 $f1 $retval]
		}
	} else {
		foreach f1 $file1 {
			foreach f2 $file2 {
				lappend retlist [list $op2 "$f1 $f2" $retval]
			}
		}
	}
	return $retlist
}

proc extract_error { message } {
	if { [is_substr $message "exists"] == 1 } {
		set message "file exists"
	} elseif {[is_substr $message "no such file"] == 1 } {
		set message "no such file"
	} elseif {[is_substr $message "file is open"] == 1 } {
		set message "file is open"
	}
	return $message
}
