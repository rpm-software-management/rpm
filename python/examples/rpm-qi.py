#!/usr/bin/python3

# Query one or more installed packages by their names but this time print more
# information.
# A Python equivalent for `rpm -qi hello`

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

    # Multiple packages can have the same name
    for hdr in mi:
        print(
            f"Name        : {hdr[rpm.RPMTAG_NAME]}\n"
            f"Version     : {hdr[rpm.RPMTAG_VERSION]}\n"
            f"Release     : {hdr[rpm.RPMTAG_RELEASE]}\n"
            f"Architecture: {hdr[rpm.RPMTAG_ARCH]}\n"
            f"Install Date: {hdr[rpm.RPMTAG_INSTALLTIME]}\n"
            f"Group       : {hdr[rpm.RPMTAG_GROUP]}\n"
            f"Size        : {hdr[rpm.RPMTAG_SIZE]}\n"
            f"License     : {hdr[rpm.RPMTAG_LICENSE]}\n"
            f"Signature   : {hdr.format("%{rsaheader:pgpsig}")}\n"
            f"Source RPM  : {hdr[rpm.RPMTAG_SOURCERPM]}\n"
            f"Build Date  : {hdr[rpm.RPMTAG_BUILDTIME]}\n"
            f"Build Host  : {hdr[rpm.RPMTAG_BUILDHOST]}\n"
            f"Packager    : {hdr[rpm.RPMTAG_PACKAGER]}\n"
            f"Vendor      : {hdr[rpm.RPMTAG_VENDOR]}\n"
            f"URL         : {hdr[rpm.RPMTAG_URL]}\n"
            f"Bug URL     : {hdr[rpm.RPMTAG_BUGURL]}\n"
            f"Summary     : {hdr[rpm.RPMTAG_SUMMARY]}\n"
            f"Description :\n{hdr[rpm.RPMTAG_DESCRIPTION]}\n"
        )
