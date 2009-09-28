r"""RPM Module

This module enables you to manipulate rpms and the rpm database.

"""

import warnings
from _rpm import *

import _rpm
_RPMVSF_NODIGESTS = _rpm._RPMVSF_NODIGESTS
_RPMVSF_NOHEADER = _rpm._RPMVSF_NOHEADER
_RPMVSF_NOPAYLOAD = _rpm._RPMVSF_NOPAYLOAD
_RPMVSF_NOSIGNATURES = _rpm._RPMVSF_NOSIGNATURES

def headerLoad(*args, **kwds):
    warnings.warn("Use rpm.hdr() instead.", DeprecationWarning, stacklevel=2)
    return hdr(*args, **kwds)

def readHeaderListFromFD(fd, retrofit = True):
    if hasattr(fd, "fileno"):
        fdno = fd.fileno()
    else:
        fdno = fd

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
    
