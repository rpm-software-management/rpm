import rpm

i = 1
#rpm.mpw().Debug(-1);
#a = rpm.mpw("0000000987654321")
#b = rpm.mpw("0000000000000010")
#c = rpm.mpw("0fedcba000000000")

for i in range(100):
	a = rpm.mpw(i)
	for j in range(100):
		b = rpm.mpw(j)
		print i, '^', j, '=', rpm.mpw.__pow__(a, b)

#rpm.mpw.__add__(a, b)
#rpm.mpw.__sub__(a, b)
#rpm.mpw.__mul__(a, b)
#rpm.mpw.__div__(a, b)
#rpm.mpw.__mod__(a, b)

#rpm.mpw.__divmod__(a, b)
#rpm.mpw.__pow__(a, b)

#rpm.mpw.__lshift__(a, b)
#rpm.mpw.__rshift__(a, b)

#rpm.mpw.__and__(a, c)
#rpm.mpw.__xor__(a, a)
#rpm.mpw.__or__(a, c)

#rpm.mpw.__neg__(a)
#rpm.mpw.__pos__(a)
#rpm.mpw.__abs__(a)
#rpm.mpw.__invert__(a)

#rpm.mpw.__int__(b)
#rpm.mpw.__long__(b)
#rpm.mpw.__float__(a)
#rpm.mpw.__complex__(b)
#rpm.mpw.__oct__(a*b)
#rpm.mpw.__hex__(a*b)
#rpm.mpw.__coerce__(b, i)

#d = rpm.mpw("987654321");	rpm.bc.__iadd__(d, b)
#d = rpm.mpw("987654321");	rpm.bc.__isub__(d, b)
#d = rpm.mpw("987654321");	rpm.bc.__imul__(d, b)
#d = rpm.mpw("987654321");	rpm.bc.__idiv__(d, b)
#d = rpm.mpw("987654321");	rpm.bc.__imod__(d, b)

#d = rpm.mpw("987654321");	rpm.bc.__ilshift__(d, b)
#d = rpm.mpw("987654321");	rpm.bc.__irshift__(d, b)

#d = rpm.mpw("987654321");	rpm.bc.__iand__(d, c)
#d = rpm.mpw("987654321");	rpm.bc.__ixor__(d, a)
#d = rpm.mpw("987654321");	rpm.bc.__ior__(d, c)

#if a > i:
#	print "a > i"

#b = rpm.mpw("123456789")
#print "b: ", a
#
#c = a + b
#print "c: ", c

