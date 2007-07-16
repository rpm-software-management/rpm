proc getstats { statlist field } {
	foreach pair $statlist {
		set txt [lindex $pair 0]
		if { [string equal $txt $field] == 1 } {
			return [lindex $pair 1]
		}
	}
	return -1
}

# Return the value for a particular field in a set of statistics.
# Works for regular db stat as well as env stats (log_stat,
# lock_stat, txn_stat, rep_stat, etc.).
proc stat_field { handle which_stat field } {
	set stat [$handle $which_stat]
	return [getstats $stat $field ]
}
