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

	print "__neg__:\t", hex(mpw.__neg__(wa)), "\t",  hex(mpz.MPZType.__neg__(za))
	print "__pos__:\t", hex(mpw.__pos__(wa)), "\t",  hex(mpz.MPZType.__pos__(za))
	print "__abs__:\t", hex(mpw.__abs__(wa)), "\t",  hex(mpz.MPZType.__abs__(za))
	print "__invert__:\t", hex(mpw.__invert__(wa)), "\t",  hex(mpz.MPZType.__invert__(za))

	print "__add__:\t", hex(mpw.__add__(wa, wb)), "\t",  hex(mpz.MPZType.__add__(za, zb))
	print "__sub__:\t", hex(mpw.__sub__(wa, wb)), "\t",  hex(mpz.MPZType.__sub__(za, zb))
	print "__mul__:\t", hex(mpw.__mul__(wa, wb)), "\t",  hex(mpz.MPZType.__mul__(za, zb))
	print "__div__:\t", hex(mpw.__div__(wa, wb)), "\t",  hex(mpz.MPZType.__div__(za, zb))
	print "__mod__:\t", hex(mpw.__mod__(wa, wb)), "\t",  hex(mpz.MPZType.__mod__(za, zb))
	wq, wr = mpw.__divmod__(wa, wb)
	zq, zr = mpz.MPZType.__divmod__(za, zb)
	print "__divmod__ q:\t", hex(wq), "\t",  hex(zq)
	print "__divmod__ r:\t", hex(wr), "\t",  hex(zr)
	print "__pow__:\t", hex(mpw.__pow__(mpw(16), wb)), "\t",  hex(mpz.MPZType.__pow__(mpz.mpz(16), zb))

	print "__lshift__:\t", hex(mpw.__lshift__(wa, wb)), "\t",  hex(mpz.MPZType.__lshift__(za, zb))
	print "__rshift__:\t", hex(mpw.__rshift__(wa, wb)), "\t",  hex(mpz.MPZType.__rshift__(za, zb))
	print "__and__:\t", hex(mpw.__and__(wa, wc)), "\t",  hex(mpz.MPZType.__and__(za, zc))
	print "__xor__:\t", hex(mpw.__xor__(wa, wa)), "\t",  hex(mpz.MPZType.__xor__(za, za))
	print "__or__:\t", hex(mpw.__or__(wa, wc)), "\t",  hex(mpz.MPZType.__or__(za, zc))

#	print mpw.__int__(b)
#	print mpw.__long__(b)
#	print mpw.__float__(a)
#	print mpw.__complex__(b)
#	print mpw.__oct__(a*b)
#	print mpw.__hex__(a*b)
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
