#See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: siutils.tcl,v 11.6 2004/03/02 18:44:41 mjc Exp $
#
# Secondary index utilities.  This file used to be known as
# sindex.tcl.
#
# The secondary index tests themselves live in si0*.tcl.
#
# Standard number of secondary indices to create if a single-element
# list of methods is passed into the secondary index tests.
global nsecondaries
set nsecondaries 2

# The callback function we use for each given secondary in most tests
# is a simple function of its place in the list of secondaries (0-based)
# and the access method (since recnos may need different callbacks).
#
# !!!
# Note that callbacks 0-3 return unique secondary keys if the input data
# are unique;  callbacks 4 and higher may not, so don't use them with
# the normal wordlist and secondaries that don't support dups.
# The callbacks that incorporate a key don't work properly with recno
# access methods, at least not in the current test framework (the
# error_check_good lines test for e.g. 1foo, when the database has
# e.g. 0x010x000x000x00foo).
proc callback_n { n } {
	switch $n {
		0 { return _s_reversedata }
		1 { return _s_noop }
		2 { return _s_concatkeydata }
		3 { return _s_concatdatakey }
		4 { return _s_reverseconcat }
		5 { return _s_truncdata }
		6 { return _s_constant }
	}
	return _s_noop
}

proc _s_reversedata { a b } { return [reverse $b] }
proc _s_truncdata { a b } { return [string range $b 1 end] }
proc _s_concatkeydata { a b } { return $a$b }
proc _s_concatdatakey { a b } { return $b$a }
proc _s_reverseconcat { a b } { return [reverse $a$b] }
proc _s_constant { a b } { return "constant data" }
proc _s_noop { a b } { return $b }

# Should the check_secondary routines print lots of output?
set verbose_check_secondaries 0

# Given a primary database handle, a list of secondary handles, a
# number of entries, and arrays of keys and data, verify that all
# databases have what they ought to.
proc check_secondaries { pdb sdbs nentries keyarr dataarr {pref "Check"} } {
	upvar $keyarr keys
	upvar $dataarr data
	global verbose_check_secondaries

	# Make sure each key/data pair is in the primary.
	if { $verbose_check_secondaries } {
		puts "\t\t$pref.1: Each key/data pair is in the primary"
	}
	for { set i 0 } { $i < $nentries } { incr i } {
		error_check_good pdb_get($i) [$pdb get $keys($i)] \
		    [list [list $keys($i) $data($i)]]
	}

	for { set j 0 } { $j < [llength $sdbs] } { incr j } {
		# Make sure each key/data pair is in this secondary.
		if { $verbose_check_secondaries } {
			puts "\t\t$pref.2:\
			    Each skey/key/data tuple is in secondary #$j"
		}
		set sdb [lindex $sdbs $j]
		for { set i 0 } { $i < $nentries } { incr i } {
			set skey [[callback_n $j] $keys($i) $data($i)]
			# Check with pget on the secondary.
			error_check_good sdb($j)_pget($i) \
			    [$sdb pget -get_both $skey $keys($i)] \
			    [list [list $skey $keys($i) $data($i)]]
			# Check again with get on the secondary.
			# Since get_both is not an allowed option
			# with get on a secondary handle, we can't
			# get an exact match on callback method 6,
			# and can't guarantee an exact match on
			# method 5.  We just make sure that one of
			# the returned key/data pairs is the right one.
			if { $j == 5 || $j == 6 } {
				error_check_good sdb($j)_get($i) \
				    [is_substr [$sdb get $skey] \
				    [list [list $skey $data($i)]]] 1
			} else {
				error_check_good sdb($j)_get($i) \
				    [$sdb get $skey] \
				    [list [list $skey $data($i)]]
			}
		}

		# Make sure this secondary contains only $nentries
		# items.
		if { $verbose_check_secondaries } {
			puts "\t\t$pref.3: Secondary #$j has $nentries items"
		}
		set dbc [$sdb cursor]
		error_check_good dbc($i) \
		    [is_valid_cursor $dbc $sdb] TRUE
		for { set k 0 } { [llength [$dbc get -next]] > 0 } \
		    { incr k } { }
		error_check_good numitems($i) $k $nentries
		error_check_good dbc($i)_close [$dbc close] 0
	}

	if { $verbose_check_secondaries } {
		puts "\t\t$pref.4: Primary has $nentries items"
	}
	set dbc [$pdb cursor]
	error_check_good pdbc [is_valid_cursor $dbc $pdb] TRUE
	for { set k 0 } { [llength [$dbc get -next]] > 0 } { incr k } { }
	error_check_good numitems $k $nentries
	error_check_good pdbc_close [$dbc close] 0
}

# Given a primary database handle and a list of secondary handles, walk
# through the primary and make sure all the secondaries are correct,
# then walk through the secondaries and make sure the primary is correct.
#
# This is slightly less rigorous than the normal check_secondaries--we
# use it whenever we don't have up-to-date "keys" and "data" arrays.
proc cursor_check_secondaries { pdb sdbs nentries { pref "Check" } } {
	global verbose_check_secondaries

	# Make sure each key/data pair in the primary is in each secondary.
	set pdbc [$pdb cursor]
	error_check_good ccs_pdbc [is_valid_cursor $pdbc $pdb] TRUE
	set i 0
	if { $verbose_check_secondaries } {
		puts "\t\t$pref.1:\
		    Key/data in primary => key/data in secondaries"
	}

	for { set dbt [$pdbc get -first] } { [llength $dbt] > 0 } \
	    { set dbt [$pdbc get -next] } {
		incr i
		set pkey [lindex [lindex $dbt 0] 0]
		set pdata [lindex [lindex $dbt 0] 1]
		for { set j 0 } { $j < [llength $sdbs] } { incr j } {
			set sdb [lindex $sdbs $j]
			# Check with pget.
			set sdbt [$sdb pget -get_both \
			    [[callback_n $j] $pkey $pdata] $pkey]
			error_check_good pkey($pkey,$j) \
			    [lindex [lindex $sdbt 0] 1] $pkey
			error_check_good pdata($pdata,$j) \
			    [lindex [lindex $sdbt 0] 2] $pdata
		}
	}
	error_check_good ccs_pdbc_close [$pdbc close] 0
	error_check_good primary_has_nentries $i $nentries

	for { set j 0 } { $j < [llength $sdbs] } { incr j } {
		if { $verbose_check_secondaries } {
			puts "\t\t$pref.2:\
			    Key/data in secondary #$j => key/data in primary"
		}
		set sdb [lindex $sdbs $j]
		set sdbc [$sdb cursor]
		error_check_good ccs_sdbc($j) [is_valid_cursor $sdbc $sdb] TRUE
		set i 0
		for { set dbt [$sdbc pget -first] } { [llength $dbt] > 0 } \
		    { set dbt [$sdbc pget -next] } {
			incr i
			set pkey [lindex [lindex $dbt 0] 1]
			set pdata [lindex [lindex $dbt 0] 2]
			error_check_good pdb_get($pkey/$pdata,$j) \
			    [$pdb get -get_both $pkey $pdata] \
			    [list [list $pkey $pdata]]
		}
		error_check_good secondary($j)_has_nentries $i $nentries

		# To exercise pget -last/pget -prev, we do it backwards too.
		set i 0
		for { set dbt [$sdbc pget -last] } { [llength $dbt] > 0 } \
		    { set dbt [$sdbc pget -prev] } {
			incr i
			set pkey [lindex [lindex $dbt 0] 1]
			set pdata [lindex [lindex $dbt 0] 2]
			error_check_good pdb_get_bkwds($pkey/$pdata,$j) \
			    [$pdb get -get_both $pkey $pdata] \
			    [list [list $pkey $pdata]]
		}
		error_check_good secondary($j)_has_nentries_bkwds $i $nentries

		error_check_good ccs_sdbc_close($j) [$sdbc close] 0
	}
}

# The secondary index tests take a list of the access methods that
# each array ought to use.  Convert at one blow into a list of converted
# argses and omethods for each method in the list.
proc convert_argses { methods largs } {
	set ret {}
	foreach m $methods {
		lappend ret [convert_args $m $largs]
	}
	return $ret
}
proc convert_methods { methods } {
	set ret {}
	foreach m $methods {
		lappend ret [convert_method $m]
	}
	return $ret
}
