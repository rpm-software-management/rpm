#!/usr/bin/python
import rpm

ts = rpm.ts()

# Grab a header
for h in ts.dbMatch('name', 'bash'):
	print h['name']
	break

print "===== rpm.ds(h)"
D = rpm.ds(h)
print D

print "===== dsFromHeader('P')"
P = h.dsFromHeader('P')
print P

print "===== dsOfHeader()"
NEVR = h.dsOfHeader()
print NEVR

print "===== dsSingle('P', 'config(bash)', '3.0-16', rpm.RPMSENSE_EQUAL)"
Pconfig = rpm.dsSingle('P', 'config(bash)', '3.0-16', rpm.RPMSENSE_EQUAL)
Dconfig = rpm.dsSingle('D', 'config(bash)', '3.0-16', rpm.RPMSENSE_EQUAL)
print Pconfig, Dconfig

print "===== ds.Merge to accomplish missing ds.Sort()"
P = rpm.dsSingle('P', "config(bash)", "3.0-16", rpm.RPMSENSE_EQUAL)
print P.Merge(NEVR)
print P

print "===== ds.Find(config) index"
print P.Find(Pconfig), P.Ix()
print D.Find(Dconfig), D.Ix()

print "===== ds.Find(NEVR) index"
print P.Find(NEVR), P.Ix()

