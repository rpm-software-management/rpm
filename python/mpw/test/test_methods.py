"""
Basic TestCases for BTree and hash DBs, with and without a DBEnv, with
various DB flags, etc.
"""

import unittest

import rpm

from test_all import verbose

DASH = '-'

Methods = ( '__add__', '__sub__', '__mul__', '__div__', '__mod__', '__lshift__', '__rshift__', '__and__', '__xor__', '__or__')

class Factory(object):
    def __init__(self, false_self, method_name):
	self.false_self = false_self
	self.method_name = method_name

    def __call__(self, val):
	xself = long(self.false_self)
	yself = int(self.false_self)
	xm = long.__getattribute__(xself, self.method_name)
	ym = rpm.mpw.__getattribute__(yself, self.method_name)
	xa = xm(long(val))
	ya = ym(int(val))
	print "  Comparing", xa, ya
	assert xa == ya
	return xa

class Long(long):
    def __getattribute__(self, name):
	print "__getattribute__ ~%s~" % name
	if name not in ('__add__', '__sub__'):
	    return long.getattr(self, name)
        return Factory(self, name)

#a1 = Bar(1)
#a2 = Bar(2)
#print a1.__add__(a2)
#print "Done"
#print a1 + a2

#----------------------------------------------------------------------

class BasicTestCase(unittest.TestCase):
    a = 0x0000000987654321L
    b = 0x0000000000000010L
    c = 0x0fedcba000000000L
    lo = 2
    hi = 200
    t = 10

    def setUp(self):
	rpm.mpw().Debug(0)
	pass

    def tearDown(self):
	rpm.mpw().Debug(0)
	pass

    #----------------------------------------

    def test01_SimpleMethods(self):
        if verbose:
            print '\n', '-=' * 30
            print "Running %s.test01_SimpleMethods..." % \
                  self.__class__.__name__
	    print "\ta:\t%s\t%s\t0x%x" % (type(self.a), self.a, self.a)
	    print "\tb:\t%s\t%s\t0x%x" % (type(self.b), self.b, self.b)
	    print "\tc:\t%s\t%s\t0x%x" % (type(self.c), self.c, self.c)

	wa = rpm.mpw(self.a)
	wb = rpm.mpw(self.b)
	wc = rpm.mpw(self.c)
#	xa - Long(self.a)
#	xb = Long(self.b)
#	xc = Long(self.c)
	za = long(self.a)
	zb = long(self.b)
	zc = long(self.c)

	print "__int__:\t", int(wb), "\t",  int(zb)
	assert int(wb) == int(zb)
	print "__long__:\t", long(wa), "\t",  long(za)
	assert long(wb) == long(zb)
	print "__float__:\t", float(wa), "\t",  float(za)
	assert float(wb) == float(zb)

	zs = hex(za)
	print "__hex__:\t", hex(wa), "\t", zs
	assert hex(wa) == zs
	zs = oct(za)
	print "__oct__:\t", oct(wa), "\t", zs
	assert oct(wa) == zs

	print "__neg__:\t", (-wa), "\t",  long(-za)
	print "__pos__:\t", (+wa), "\t",  long(+za)
	print "__abs__:\t", abs(wa), "\t",  long(abs(za))
	print "__invert__:\t", (~wa), "\t",  long(~za)

	print "__add__:\t", (wa + wb), "\t",  long(za + zb)
	print "__sub__:\t", (wa - wb), "\t",  long(za - zb)
	print "__mul__:\t", (wa * wb), "\t",  long(za * zb)
	print "__div__:\t", (wa / wb), "\t",  long(za / zb)
	print "__mod__:\t", (wa % wb), "\t",  long(za % zb)

	wq, wr = divmod(wa, wb)
	zq, zr = divmod(za, zb)
	print "__divmod__ q:\t", wq, "\t",  long(zq)
	print "__divmod__ r:\t", wr, "\t",  long(zr)

	print "__pow__:\t", (wb ** wb), "\t",  long(zb ** zb)

	print "__lshift__:\t", (wa << wb), "\t",  long(za << zb)
	print "__rshift__:\t", (wa >> wb), "\t",  long(za >> zb)
	print "__and__:\t", (wa & wc), "\t",  long(za & zc)
	print "__xor__:\t", (wa ^ wa), "\t",  long(za ^ za)
	print "__or__:\t", (wa | wc), "\t",  long(za | zc)

#	print rpm.mpw.__complex__(b)
#	print rpm.mpw.__coerce__(b, i)

	del wa
	del wb
	del wc
	del za
	del zb
	del zc
	pass

    #----------------------------------------
    def test02_CarryBorrow(self):
        if verbose:
            print '\n', '-=' * 30
            print "Running %s.test02_CarryBorrow..." % \
                  self.__class__.__name__
	a = 0x7fffffff
	wa = -rpm.mpw(a); wa = wa+wa
	za = -long(a); za = za+za
	wb = -rpm.mpw(1)
	zb = -long(1)
	wc = rpm.mpw(1)
	zc = long(1)
	wd = rpm.mpw(a); wd = wd+wd
	zd = long(a); zd = zd+zd
	print "add --:\t", (wa+wa), "\t",  (za+za)
	print "add -+:\t", (wb+wd), "\t",  (zb+zd)
	print "add +-:\t", (wc+wa), "\t",  (zc+za)
	print "add ++:\t", (wd+wd), "\t",  (zd+zd)
	print "sub --:\t", (wb-wa), "\t",  (zb-za)
#	print "sub -+:\t", (wb-wd), "\t",  (zb-zd)
#	print "sub +-:\t", (wc-wa), "\t",  (zc-za)
	print "sub ++:\t", (wc-wd), "\t",  (zc-zd)
	pass

    #----------------------------------------
    def test03_Signs(self):
        if verbose:
            print '\n', '-=' * 30
            print "Running %s.test03_Signs..." % \
                  self.__class__.__name__
	wp = rpm.mpw(11)
	wm = -wp
	wpp = wp + 1
	wmm = -wpp
	zp = long(11)
	zm = -zp
	zpp = zp + 1
	zmm = -zpp
	print "add --:\t", (wm+wm), "\t", (zm+zm)
	print "add -+:\t", (wm+wp), "\t", (zm+zp)
	print "add +-:\t", (wp+wm), "\t", (zp+zm)
	print "add ++:\t", (wp+wp), "\t", (zp+zp)
	print "sub --:\t", (wm-wm), "\t", (zm-zm)
	print "sub -+:\t", (wm-wp), "\t", (zm-zp)
	print "sub +-:\t", (wp-wm), "\t", (zp-zm)
	print "sub ++:\t", (wp-wp), "\t", (zp-zp)
	print "mul --:\t", (wm*wm), "\t", (zm*zm)
	print "mul -+:\t", (wm*wp), "\t", (zm*zp)
	print "mul +-:\t", (wp*wm), "\t", (zp*zm)
	print "mul ++:\t", (wp*wp), "\t", (zp*zp)
	print "div --:\t", (wm/wm), "\t", (zm/zm)
	print "div -+:\t", (wm/wp), "\t", (zm/zp)
	print "div +-:\t", (wp/wm), "\t", (zp/zm)
	print "div ++:\t", (wp/wp), "\t", (zp/zp)

	print "pow --:\t", (wm**wm), "\t", (zm**zm)
	print "pow -+:\t", (wm**wp), "\t", (zm**zp)
	print "pow +-:\t", (wp**wm), "\t", (zp**zm)
	print "pow ++:\t", (wp**wp), "\t", (zp**zp)

	wpa = rpm.mpw(13)
	wma = -wpa
	wpb = wpa - 3
	wmb = -wpb
	zpa = long(13)
	zma = -zpa
	zpb = zpa - 3
	zmb = -zpb
	print "mod --:\t", (wma%wmb), "\t", (zma%zmb)
	print "mod -+:\t", (wma%wpb), "\t", (zma%zpb)
	print "mod +-:\t", (wpa%wmb), "\t", (zpa%zmb)
	print "mod ++:\t", (wpa%wpb), "\t", (zpa%zpb)

	print "rem --:\t", divmod(wma, wmb), "\t", divmod(zma, zmb)
	print "rem -+:\t", divmod(wma, wpb), "\t", divmod(zma, zpb)
	print "rem +-:\t", divmod(wpa, wmb), "\t", divmod(zpa, zmb)
	print "rem ++:\t", divmod(wpa, wpb), "\t", divmod(zpa, zpb)
	pass

    #----------------------------------------
    def test04_KnuthPoly(self):
	self.t = 8
	tfmt = "%o"
        if verbose:
            print '\n', '-=' * 30
            print "Running %s.test04_KnuthPoly..." % \
                  self.__class__.__name__
	    print "\t(%d**m - 1) * (%d**n - 1), m,n in [%d,%d)" % (self.t,self.t,self.lo,self.hi)
	tm1 = tfmt % (self.t - 1)
	tm2 = tfmt % (self.t - 2)
	for m in range(self.lo,self.hi):
	    for n in range(m+1,self.hi+1):
		wt = rpm.mpw(self.t)
		wa = (wt**m - 1) * (wt**n - 1)
		ws = tfmt % long(wa)
		zs = tm1 * (m - 1) + tm2 + tm1 * (n - m) + "0" * (m - 1) + "1"
		if ws != zs:
		    print "(%d**%d - 1) * (%d**%d - 1)\t%s" % (self.t,m,self.t,n,ws)
		assert ws == zs

	self.t = 10
	tfmt = "%d"
        if verbose:
	    print "\t(%d**m - 1) * (%d**n - 1), m,n in [%d,%d)" % (self.t,self.t,self.lo,self.hi)
	tm1 = tfmt % (self.t - 1)
	tm2 = tfmt % (self.t - 2)
	for m in range(self.lo,self.hi):
	    for n in range(m+1,self.hi+1):
		wt = rpm.mpw(self.t)
		wa = (wt**m - 1) * (wt**n - 1)
		ws = tfmt % long(wa)
		zs = tm1 * (m - 1) + tm2 + tm1 * (n - m) + "0" * (m - 1) + "1"
		if ws != zs:
		    print "(%d**%d - 1) * (%d**%d - 1)\t%s" % (self.t,m,self.t,n,ws)
		assert ws == zs

	self.t = 16
	tfmt = "%x"
        if verbose:
	    print "\t(%d**m - 1) * (%d**n - 1), m,n in [%d,%d)" % (self.t,self.t,self.lo,self.hi)
	tm1 = tfmt % (self.t - 1)
	tm2 = tfmt % (self.t - 2)
	for m in range(self.lo,self.hi):
	    for n in range(m+1,self.hi+1):
		wt = rpm.mpw(self.t)
		wa = (wt**m - 1) * (wt**n - 1)
		ws = tfmt % long(wa)
		zs = tm1 * (m - 1) + tm2 + tm1 * (n - m) + "0" * (m - 1) + "1"
		if ws != zs:
		    print "(%d**%d - 1) * (%d**%d - 1)\t%s" % (self.t,m,self.t,n,ws)
		assert ws == zs
	pass

    #----------------------------------------
    def test05_IterativePowers(self):
        if verbose:
            print '\n', '-=' * 30
            print "Running %s.test05_IterativePowers..." % \
                  self.__class__.__name__
	    print "\t(m**n)/(m**(n-1)) == m for m,n in [%d,%d)" % (self.lo,self.hi)
	for m in range(self.lo,self.hi):
	    wa = rpm.mpw(m)
	    wd = wa
	    for n in range(self.lo,self.hi):
		wc = wa**n
		we = wc/wd
		if we != m:
		    print m, '^', n, '=', we
		assert we == m
		if wc != 0:
		    wd = wc
	pass

#----------------------------------------------------------------------
#----------------------------------------------------------------------

def test_suite():
    suite = unittest.TestSuite()

    suite.addTest(unittest.makeSuite(BasicTestCase))

    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
