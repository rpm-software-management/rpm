"""
Run all test cases.
"""

import sys
import unittest

verbose = 0
if 'verbose' in sys.argv:
    verbose = 1
    sys.argv.remove('verbose')

if 'silent' in sys.argv:  # take care of old flag, just in case
    verbose = 0
    sys.argv.remove('silent')


# This little hack is for when this module is run as main and all the
# other modules import it so they will still be able to get the right
# verbose setting.  It's confusing but it works.
import test_all
test_all.verbose = verbose


def suite():
    test_modules = [ 'test_compat',
                     'test_basics',
                     'test_misc',
                     'test_dbobj',
                     'test_recno',
                     'test_queue',
                     'test_get_none',
                     'test_dbshelve',
                     'test_dbtables',
                     'test_thread',
                     'test_lock',
                     'test_associate',
                   ]

    alltests = unittest.TestSuite()
    for name in test_modules:
        module = __import__(name)
        alltests.addTest(module.suite())
    return alltests


if __name__ == '__main__':
    from rpmdb import db
    print '-=' * 38
    print db.DB_VERSION_STRING
    print 'rpmdb.db.version():   %s' % (db.version(), )
    print 'rpmdb.db.__version__: %s' % db.__version__
    print 'rpmdb.db.cvsid:       %s' % db.cvsid
    print 'python version:        %s' % sys.version
    print '-=' * 38

    unittest.main( defaultTest='suite' )

