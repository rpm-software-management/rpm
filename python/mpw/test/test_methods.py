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
            print "Running %s.test01_GetsAndPuts..." % \
                  self.__class__.__name__

	wa = mpw("0000000987654321")
	wb = mpw("0000000000000010")
	wc = mpw("0fedcba000000000")
	za = mpz.mpz(0x0000000987654321)
	zb = mpz.mpz(0x0000000000000010)
	zc = mpz.mpz(0x0fedcba000000000)

	print hex(mpw.__add__(wa, wb)), hex(mpz.MPZType.__add__(za, zb))
	print hex(mpw.__sub__(wa, wb)), hex(mpz.MPZType.__sub__(za, zb))
	print hex(mpw.__mul__(wa, wb)), hex(mpz.MPZType.__mul__(za, zb))
	print hex(mpw.__div__(wa, wb)), hex(mpz.MPZType.__div__(za, zb))
	print hex(mpw.__mod__(wa, wb)), hex(mpz.MPZType.__mod__(za, zb))

#	print mpw.__divmod__(a, b)
#	print mpw.__pow__(a, b)

#	print mpw.__lshift__(a, b)
#	print mpw.__rshift__(a, b)

#	print mpw.__and__(a, c)
#	print mpw.__xor__(a, a)
#	print mpw.__or__(a, c)

#	print mpw.__neg__(a)
#	print mpw.__pos__(a)
#	print mpw.__abs__(a)
#	print mpw.__invert__(a)

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
