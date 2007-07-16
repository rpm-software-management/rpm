# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: test027.tcl,v 12.3 2006/08/24 14:46:40 bostic Exp $
#
# TEST	test027
# TEST	Off-page duplicate test
# TEST		Test026 with parameters to force off-page duplicates.
# TEST
# TEST	Check that delete operations work.  Create a database; close
# TEST	database and reopen it.  Then issues delete by key for each
# TEST	entry.
proc test027 { method {nentries 100} args} {
	eval {test026 $method $nentries 100 "027"} $args
}
