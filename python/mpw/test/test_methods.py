"""
Basic TestCases for BTree and hash DBs, with and without a DBEnv, with
various DB flags, etc.
"""

import os
import sys
import errno
import shutil
import string
import tempfile
from pprint import pprint
import unittest

from rpm import mpw
import mpz

from test_all import verbose

DASH = '-'


#----------------------------------------------------------------------

class BasicTestCase(unittest.TestCase):

    def setUp(self):
	mpw().Debug(0)
	pass

    def tearDown(self):
	mpw().Debug(0)
	pass

    #----------------------------------------

    def test01_SimpleMethods(self):
        if verbose:
            print '\n', '-=' * 30
            print "Running %s.test01_SimpleMethods..." % \
                  self.__class__.__name__

	wa = mpw("0000000987654321")
	wb = mpw("0000000000000010")
	wc = mpw("0fedcba000000000")
	za = mpz.mpz(0x0000000987654321)
	zb = mpz.mpz(0x0000000000000010)
	zc = mpz.mpz(0x0fedcba000000000)

	print "__int__:\t", int(wb), "\t",  int(zb)
	print "__long__:\t", long(wb), "\t",  long(zb)
	print "__float__:\t", float(wb), "\t",  float(zb)

	zs = hex(za)
	zs = zs[4:len(zs)-1]
	print "__hex__:\t", hex(wa), "\t", zs
	zs = oct(za)
	zs = zs[4:len(zs)-1]
	print "__oct__:\t", oct(wa), "\t", zs

	print "__neg__:\t", (-wa), "\t",  long(-za)
	print "__pos__:\t", (+wa), "\t",  long(mpz.MPZType.__pos__(za))
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

#	print mpw.__complex__(b)
#	print mpw.__coerce__(b, i)

	del wa
	del wb
	del wc
	del za
	del zb
	del zc
	pass

    #----------------------------------------

#----------------------------------------------------------------------
#----------------------------------------------------------------------

def test_suite():
    suite = unittest.TestSuite()

    suite.addTest(unittest.makeSuite(BasicTestCase))

    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
