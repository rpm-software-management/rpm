import rpm

#rpm.mpw().Debug(-1);
for i in range(100):
    a = rpm.mpw(i)
    for j in range(100):
	c = a**j
	print i, '^', j, '=', c
