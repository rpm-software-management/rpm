#!/usr/bin/python3

# Query all packages that match part of their name with the searched string.
# If package name isn't specified, query all installed packages.
# A Python equivalent for `rpm -qa kernel*` and `rpm -qa`

import sys
import rpm

ts = rpm.TransactionSet()
mi = ts.dbMatch()

for name in sys.argv[1:]:
    mi.pattern("name", rpm.RPMMIRE_GLOB, name)

for hdr in mi:
    print(hdr[rpm.RPMTAG_NVRA])
