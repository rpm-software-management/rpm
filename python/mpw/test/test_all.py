"""Run all test cases.
"""

import sys
import os
import unittest

verbose = 0
if 'verbose' in sys.argv:
    verbose = 1
    sys.argv.remove('verbose')

if 'silent' in sys.argv:  # take care of old flag, just in case
    verbose = 0
    sys.argv.remove('silent')


def print_versions():
    from rpm import mpw
    print
    print '-=' * 38
    print 'python version:       %s' % sys.version
    print 'My pid:               %s' % os.getpid()
    print '-=' * 38


class PrintInfoFakeTest(unittest.TestCase):
    def testPrintVersions(self):
        print_versions()


# This little hack is for when this module is run as main and all the
# other modules import it so they will still be able to get the right
# verbose setting.  It's confusing but it works.
import test_all
test_all.verbose = verbose


def suite():
    test_modules = [
        'test_methods',
        ]

    alltests = unittest.TestSuite()
    for name in test_modules:
        module = __import__(name)
        alltests.addTest(module.test_suite())
    return alltests


def test_suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(PrintInfoFakeTest))
    return suite


if __name__ == '__main__':
    print_versions()
    unittest.main(defaultTest='suite')
