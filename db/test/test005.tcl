# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: test005.tcl,v 12.3 2006/08/24 14:46:39 bostic Exp $
#
# TEST	test005
# TEST	Small keys/medium data
# TEST		Put/get per key
# TEST		Close, reopen
# TEST		Sequential (cursor) get/delete
# TEST
# TEST	Check that cursor operations work.  Create a database; close
# TEST	it and reopen it.  Then read through the database sequentially
# TEST	using cursors and delete each element.
proc test005 { method {nentries 10000} args } {
	eval {test004 $method $nentries "005" 0} $args
}
