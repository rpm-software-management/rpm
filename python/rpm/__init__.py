r"""RPM Module

This module enables you to manipulate rpms and the rpm database.

"""

import warnings
import os
from _rpm import *

import _rpm
_RPMVSF_NODIGESTS = _rpm._RPMVSF_NODIGESTS
_RPMVSF_NOHEADER = _rpm._RPMVSF_NOHEADER
_RPMVSF_NOPAYLOAD = _rpm._RPMVSF_NOPAYLOAD
_RPMVSF_NOSIGNATURES = _rpm._RPMVSF_NOSIGNATURES

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
