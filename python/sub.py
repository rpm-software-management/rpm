class Factory(object):
    def __init__(self, false_self, method_name):
	self.false_self = false_self
	self.method_name = method_name

    def __call__(self, val):
	lself = long(self.false_self)
	iself = int(self.false_self)
	lm = long.__getattribute__(lself, self.method_name)
	im = int.__getattribute__(iself, self.method_name)
	la = lm(long(val))
	ia = im(int(val))
	print "  Comparing", la, ia
	assert la == ia
	return la

class Bar(long):
    def __getattribute__(self, name):
	print "__getattribute__ ~%s~" % name
	if name not in ('__add__', '__sub__', ):
	    return long.getattr(self, name)
        return Factory(self, name)

a1 = Bar(1)
a2 = Bar(2)
print a1.__add__(a2)
print "Done"
print a1 + a2
