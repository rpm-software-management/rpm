#!/usr/bin/python


import rpm

import time

ts = rpm.TransactionSet()
ts.Debug(-1)
for x in range(100):
	tidlist = ts.IDTXload()
	time.sleep(3)
	print "id(tidlist): %s" % id(tidlist)
	del tidlist
