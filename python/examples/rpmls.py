#!/usr/bin/python3

# A simple rpm file list demonstration.
# Accepts both installed and package files as arguments, lists contents
# in `ls` style.

import rpm
import sys
import stat
import time

def ls(h):
    for f in rpm.files(h):
        mode = stat.filemode(f.mode)
        mtime = time.strftime('%b %e %H:%M', time.localtime(f.mtime))
        print('%s %2d %s %s %8d %s %s' %
              (mode, f.nlink, f.user, f.group, f.size, mtime, f.name))

if __name__ == '__main__':
    ts = rpm.ts()
    for arg in sys.argv[1:]:
        if arg.endswith('.rpm'):
            h = ts.hdrFromFdno(arg)
            ls(h)
        else:
            for h in ts.dbMatch('name', arg):
                ls(h)
