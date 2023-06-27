#!/usr/bin/python3

# A simple rpm unpacking demonstration.
# Accepts package files as arguments and unpacks them relative to current
# directory, similar to 'rpm2cpio <pkg> | cpio -idv'.

import rpm
import sys
import stat
import os

def unpack(h, fd):
    files = rpm.files(h)
    payload = rpm.fd(fd, 'r', h['payloadcompressor'])
    archive = files.archive(payload)
    for f in archive:
        os.makedirs('./%s' % f.dirname, exist_ok=True)
        fn = './%s' % f.name
        if stat.S_ISREG(f.mode):
            if archive.hascontent():
                wfd = rpm.fd(fn, 'w')
                archive.readto(wfd)
                wfd.close()
                # handle hardlinks
                if f.nlink > 1:
                    for l in f.links:
                        if l.name != f.name:
                            ln = './%s' % l.name
                            os.link(fn, ln)
        elif stat.S_ISDIR(f.mode):
            os.mkdir(fn)
        elif stat.S_ISLNK(f.mode):
            os.symlink(f.linkto, fn)

if __name__ == '__main__':
    ts = rpm.ts()
    for arg in sys.argv[1:]:
        fd = rpm.fd(arg, 'r')
        h = ts.hdrFromFdno(fd)
        unpack(h, fd)
