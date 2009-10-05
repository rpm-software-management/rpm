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

def _fdno(fd):
    if hasattr(fd, "fileno"):
        return fd.fileno()
    else:
        return fd

def readHeaderListFromFD(fd, retrofit = True):
    fdno = _fdno(fd)
    hlist = []
    while 1:
        try:
            h = hdr(fdno)
            if retrofit:
                h.convert(HEADERCONV_RETROFIT_V3)
            hlist.append(h)
        except _rpm.error:
            break

    return hlist
        
def readHeaderListFromFile(path):
    f = open(path)
    hlist = readHeaderListFromFD(f)
    f.close()
    return hlist
    
def readHeaderFromFD(fd):
    fdno = _fdno(fd)
    offset = os.lseek(fdno, 0, os.SEEK_CUR)
    try:
        h = hdr(fdno)
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
