import rpm

rpm.rng().Debug(-1);
#rpm.mpw().Debug(-1);

r = rpm.rng()
for i in range(10):
	print r.next()

m = rpm.mpw("800000000000000000000000000000000000000000000000000000000000000000000000")
for i in range(10):
	print r.next(m)
