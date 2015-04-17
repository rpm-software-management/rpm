r"""RPM Module

This module enables you to manipulate rpms and the rpm database.

The rpm base module provides the main starting point for
accessing RPM from Python. For most usage, call
the TransactionSet method to get a transaction set (rpmts).

For example:
	import rpm
	ts = rpm.TransactionSet()

The transaction set will open the RPM database as needed, so
in most cases, you do not need to explicitly open the
database. The transaction set is the workhorse of RPM.

You can open another RPM database, such as one that holds
all packages for a given Linux distribution, to provide
packages used to solve dependencies. To do this, use
the following code:

rpm.addMacro('_dbpath', '/path/to/alternate/database')
solvets = rpm.TransactionSet()
solvets.openDB()
rpm.delMacro('_dbpath')

# Open default database
ts = rpm.TransactionSet()

This code gives you access to two RPM databases through
two transaction sets (rpmts): ts is a transaction set
associated with the default RPM database and solvets
is a transaction set tied to an alternate database, which
is very useful for resolving dependencies.
"""

import warnings
from rpm._rpm import *
from rpm.transaction import *
import rpm._rpm as _rpm
_RPMVSF_NODIGESTS = _rpm._RPMVSF_NODIGESTS
_RPMVSF_NOHEADER = _rpm._RPMVSF_NOHEADER
_RPMVSF_NOPAYLOAD = _rpm._RPMVSF_NOPAYLOAD
_RPMVSF_NOSIGNATURES = _rpm._RPMVSF_NOSIGNATURES

__version__ = _rpm.__version__
__version_info__ = tuple(__version__.split('.'))

# try to import build bits but dont require it
try:
    from rpm._rpmb import *
except ImportError:
    pass

# try to import signing bits but dont require it
try:
    from rpm._rpms import *
except ImportError:
    pass

# backwards compatibility + give the same class both ways
ts = TransactionSet


def headerLoad(*args, **kwds):
    """DEPRECATED! Use rpm.hdr() instead."""
    warnings.warn("Use rpm.hdr() instead.", DeprecationWarning, stacklevel=2)
    return hdr(*args, **kwds)


def _doHeaderListFromFD(rpm_fd, retrofit):
    hlist = []
    while 1:
        try:
            h = hdr(rpm_fd)
            if retrofit:
                h.convert(HEADERCONV_RETROFIT_V3)
            hlist.append(h)
        except _rpm.error:
            break

    return hlist


def readHeaderListFromFD(file_desc, retrofit=True):
    if not isinstance(file_desc, fd):
        file_desc = fd(file_desc)
    return _doHeaderListFromFD(file_desc, retrofit)


def readHeaderListFromFile(path, retrofit=True):
    f = fd(path)
    hlist = _doHeaderListFromFD(f, retrofit)
    f.close()
    return hlist


def readHeaderFromFD(file_desc):
    """Return (header, pos_before_hdr)"""
    if not isinstance(file_desc, fd):
        file_desc = fd(file_desc)
    try:
        offset = file_desc.tell()
        h = hdr(file_desc)
    except (_rpm.error, IOError):
        offset = None
        h = None

    return (h, offset)


def signalsCaught(siglist):
    """Returns True if any of the signals was caught."""
    caught = []
    for sig in siglist:
        if signalCaught(sig):
            caught.append(sig)

    return caught


def dsSingle(TagN, N, EVR="", Flags=RPMSENSE_ANY):
    """
    Creates a single entry dependency set (ds)

    dsSingle(RPMTAG_CONFLICTNAME, "rpm") correspons to "Conflicts: rpm"
    """
    return ds((N, Flags, EVR), TagN)
