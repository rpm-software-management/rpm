import rpm

rpm.mpw().Debug(1)

Methods = ( '__neg__', '__pos__', '__abs__', '__invert__', '__add__', '__sub__', '__mul__', '__div__', '__mod__', '__divmod__', '__pow__', '__lshift__', '__rshift__', '__and__', '__xor__', '__or__')

class Factory(object):
    def __init__(self, false_self, method_name):
	self.false_self = false_self
	self.method_name = method_name

    def __call__(self, val):
	xself = long(self.false_self)
	yself = rpm.mpw(self.false_self)
	xm = long.__getattribute__(xself, self.method_name)
	ym = rpm.mpw.__getattribute__(yself, self.method_name)
	xa = xm(long(val))
	ya = ym(rpm.mpw(val))
	print "  Comparing", self.method_name, type(xa), xa, type(ya), ya
	assert xa == ya
	return xa

class Long(long):
    def __getattribute__(self, name):
	print "__getattribute__ ~%s~" % name
	if name not in Methods:
	    return long.getattr(self, name)
        return Factory(self, name)

a1 = Long(0x987654321)
a2 = Long(0x10)
a3 = Long(0x0fedcba000000000)

#print a1.__neg__()
#print a1.__pos__()
#print a1.__abs__()
#print a1.__invert__()

print a1.__add__(a2)
print a1.__sub__(a2)
print a1.__mul__(a2)
print a1.__div__(a2)
print a1.__mod__(a2)

#print a1.__divmod(a2)

print a1.__pow__(a2)

print a1.__lshift__(a2)
print a1.__rshift__(a2)
print a1.__and__(a3)
print a1.__xor__(a3)
print a1.__or__(a3)
