# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2001
#	Sleepycat Software.  All rights reserved.
#
# Id: lock004.tcl,v 11.1 2001/10/10 16:22:10 ubell Exp 
#
# TEST	lock004
# TEST	Test locker ids wraping around.

proc lock004 {} {
	source ./include.tcl
	global lock_curid
	global lock_maxid

	puts "Lock004.a -- locker id wrapping"
	locktest -S [expr $lock_maxid - 1] $lock_maxid

	puts "Lock004.b -- test out of locker ids"
	env_cleanup $testdir

	# Open/create the lock region
	set e [berkdb env -create -lock -home $testdir]
	error_check_good env_open [is_substr $e env] 1

	catch { $e lock_id } locker1
	error_check_good locker1 [is_valid_locker $locker1] TRUE
	error_check_good lock_id_set \
	    [$e lock_id_set [expr $lock_maxid - 1] $lock_maxid] 0
	
	catch { $e lock_id } locker2
	error_check_good locker2 [is_valid_locker $locker2] TRUE
	catch { $e lock_id } locker3
	error_check_bad locker3 [is_valid_locker $locker3] TRUE
	error_check_good locker3 [string match "*wrapped*" $locker3] 1

	catch { $e lock_id_free $locker1 } ret
	error_check_good free $ret 0
	catch { $e lock_id } locker4
	error_check_good locker4 [is_valid_locker $locker4] TRUE

	catch { $e lock_id_free $locker2 } ret
	error_check_good free $ret 0
	catch { $e lock_id_free $locker4 } ret
	error_check_good free $ret 0

	catch {$e close} ret
	error_check_good close $ret 0
}
