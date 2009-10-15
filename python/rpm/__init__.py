r"""RPM Module

This module enables you to manipulate rpms and the rpm database.

"""

import warnings
import os
from _rpm import *
from transaction import *

import _rpm
_RPMVSF_NODIGESTS = _rpm._RPMVSF_NODIGESTS
_RPMVSF_NOHEADER = _rpm._RPMVSF_NOHEADER
_RPMVSF_NOPAYLOAD = _rpm._RPMVSF_NOPAYLOAD
_RPMVSF_NOSIGNATURES = _rpm._RPMVSF_NOSIGNATURES

# try to import build bits but dont require it
try:
    from _rpmb import *
except ImportError:
    pass

# backwards compatibility + give the same class both ways
ts = TransactionSet

def headerLoad(*args, **kwds):
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

def readHeaderListFromFD(file_desc, retrofit = True):
    if not isinstance(file_desc, fd):
        file_desc = fd(file_desc)
    return _doHeaderListFromFD(file_desc, retrofit)
        
def readHeaderListFromFile(path, retrofit = True):
    f = fd(path)
    hlist = _doHeaderListFromFD(f, retrofit)
    f.close()
    return hlist
    
def readHeaderFromFD(file_desc):
    if not isinstance(file_desc, fd):
        file_desc = fd(file_desc)
    offset = os.lseek(file_desc.fileno(), 0, os.SEEK_CUR)
    try:
        h = hdr(file_desc)
    except _rpm.error:
        h = None
        offset = None

    return (h, offset)

def signalsCaught(siglist):
    caught = []
    for sig in siglist:
        if signalCaught(sig):
            caught.append(sig)

    return caught

def dsSingle(TagN, N, EVR = "", Flags = RPMSENSE_ANY):
    return ds((N, EVR, Flags), TagN)
