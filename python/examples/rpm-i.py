#!/usr/bin/python3

# Install a package from a file stored on your system.
# A Python equivalent for `rpm -i hello-2.12.1-4.fc40.x86_64.rpm`

import os
import sys
import rpm


class Callback:
    def __init__(self):
        self.fdnos = {}

    def callback(self, what, amount, total, mydata, wibble):
        if what == rpm.RPMCALLBACK_INST_OPEN_FILE:
            nvr, path = mydata
            fd = os.open(path, os.O_RDONLY)
            self.fdnos[nvr] = fd
            return fd

        elif what == rpm.RPMCALLBACK_INST_CLOSE_FILE:
            nvr, path = mydata
            os.close(self.fdnos[nvr])


ts = rpm.TransactionSet()
for path in sys.argv[1:]:
    with open(path, "r") as fp:
        hdr = ts.hdrFromFdno(fp.fileno())
        ts.addInstall(hdr, (hdr.nvr, path), "u")

callback = Callback()
ts.run(callback.callback, "")
