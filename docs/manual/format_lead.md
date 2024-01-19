---
layout: default
title: rpm.org - RPM Package Lead format
---

## Lead Format

The Lead is stored as a C structure, all integer data stored in
the network byte order:

```
struct rpmlead {
    unsigned char magic[4];
    unsigned char major, minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;
    char reserved[16];
};
```

and is illustrated with one pulled from the rpm-2.1.2-1.i386.rpm
package:

```
00000000: ed ab ee db 03 00 00 00
```

The first 4 bytes (0-3) are "magic" used to uniquely identify an RPM
package.  It is used by RPM and file(1).  The next two bytes (4, 5)
are int8 quantities denoting the "major" and "minor" RPM file format
version.  This package is in 3.0 format.  The following 2 bytes (6-7)
form an int16 which indicates the package type.  As of this writing
there are only two types: 0 == binary, 1 == source.

```
00000008: 00 01 72 70 6d 2d 32 2e    ..rpm-2.
```

The next two bytes (8-9) form an int16 that indicates the architecture
the package was built for.  While this is used by file(1), the true
architecture is stored as a string in the Header.  See `rpmrc` for
a list of architecture->int16 translations.  In this case, 1 == i386.
Starting with byte 10 and extending to byte 75, are 65 characters and
a null byte which contain the familiar "name-version-release" of the
package, padded with null (0) bytes.

```
00000010: 31 2e 32 2d 31 00 00 00    1.2-1...
00000018: 00 00 00 00 00 00 00 00    ........
00000020: 00 00 00 00 00 00 00 00    ........
00000028: 00 00 00 00 00 00 00 00    ........
00000030: 00 00 00 00 00 00 00 00    ........
00000038: 00 00 00 00 00 00 00 00    ........
00000040: 00 00 00 00 00 00 00 00    ........
00000048: 00 00 00 00 00 01 00 05    ........
```

Bytes 76-77 ("00 01" above) form an int16 that indicates the OS the
package was built for.  In this case, 1 == Linux.  The next 2 bytes
(78-79) form an int16 that indicates the signature type.  This tells
RPM what to expect in the Signature.  For version 3.0 packages, this
is 5, which indicates the new "Header-style" signatures.

```
00000050: 04 00 00 00 68 e6 ff bf    ........
00000058: ab ad 00 08 3c eb ff bf    ........
```

The remaining 16 bytes (80-95) are unused and reserved.
