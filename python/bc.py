import rpm

i = 1
a = rpm.bc("987654321")
b = rpm.bc("10")
c = rpm.bc("fedcba000000000")

rpm.bc.__add__(a, b)
rpm.bc.__sub__(a, b)
rpm.bc.__mul__(a, b)
rpm.bc.__div__(a, b)
rpm.bc.__mod__(a, b)

#rpm.bc.__divmod__(a, b)
#rpm.bc.__pow__(a, b)

rpm.bc.__lshift__(a, b)
rpm.bc.__rshift__(a, b)

rpm.bc.__and__(a, c)
rpm.bc.__xor__(a, a)
rpm.bc.__or__(a, c)

rpm.bc.__neg__(a)
rpm.bc.__pos__(a)
rpm.bc.__abs__(a)
rpm.bc.__invert__(a)

rpm.bc.__int__(b)
rpm.bc.__long__(b)
#rpm.bc.__float__(b)
#rpm.bc.__complex__(b)
#rpm.bc.__oct__(b)
#rpm.bc.__hex__(b)
rpm.bc.__coerce__(b, i)

d = rpm.bc("987654321");	rpm.bc.__iadd__(d, b)
d = rpm.bc("987654321");	rpm.bc.__isub__(d, b)
d = rpm.bc("987654321");	rpm.bc.__imul__(d, b)
d = rpm.bc("987654321");	rpm.bc.__idiv__(d, b)
d = rpm.bc("987654321");	rpm.bc.__imod__(d, b)

d = rpm.bc("987654321");	rpm.bc.__ilshift__(d, b)
d = rpm.bc("987654321");	rpm.bc.__irshift__(d, b)

d = rpm.bc("987654321");	rpm.bc.__iand__(d, c)
d = rpm.bc("987654321");	rpm.bc.__ixor__(d, a)
d = rpm.bc("987654321");	rpm.bc.__ior__(d, c)

#b = rpm.bc("123456789")
#print "b: ", a
#
#c = a + b
#print "c: ", c

