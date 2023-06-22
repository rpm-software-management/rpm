#!/usr/bin/python3

# A simple rpm unpacking demonstration.
# Accepts src.rpm files as arguments and extracts spec files to current
# directory.

import rpm
import sys

def getspec(h, fd):
    files = rpm.files(h)
    payload = rpm.fd(fd, 'r', h['payloadcompressor'])
    archive = files.archive(payload)
    for f in archive:
        if not f.fflags & rpm.RPMFILE_SPECFILE:
            continue
        fn = './%s' % f.name
        wfd = rpm.fd(fn, 'w')
        archive.readto(wfd)
        wfd.close()
        print(fn, file=sys.stderr)
        break

if __name__ == '__main__':
    ts = rpm.ts()
    for arg in sys.argv[1:]:
        fd = rpm.fd(arg, 'r')
        h = ts.hdrFromFdno(fd)
        if not h.isSource():
            print("%s: not a source rpm" % arg, file=sys.stderr)
            continue
        getspec(h, fd)
