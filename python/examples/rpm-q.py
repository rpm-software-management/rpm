#!/usr/bin/python

# Query one or more installed packages by their names.
# A Python equivalent for `rpm -q hello`

import sys
import rpm

if len(sys.argv) == 1:
    print("rpm: no arguments given for query")
    sys.exit(1)

ts = rpm.TransactionSet()
for name in sys.argv[1:]:
    mi = ts.dbMatch("name", name)
    if not mi:
        print("package {0} is not installed".format(name))
        continue

    # Multiple packages can have the same name
    for hdr in mi:
        print(hdr[rpm.RPMTAG_NVRA])
