#!/usr/bin/python
# Look up file ownerships from the rpmdb
# Usage: ./fnlookup.py <file1> [...]

import rpm
import sys

ts = rpm.ts()
for arg in sys.argv[1:]:
    for h in ts.dbMatch('basenames', arg):
        print(h['nevra'])
