---
layout: default
title: rpm.org - RPM Package format
---
# Package format

This document describes the RPM file format version 3.0, which is used
by RPM versions 2.1 and greater.  The format is subject to change, and
you should not assume that this document is kept up to date with the
latest RPM code.  That said, the 3.0 format should not change for
quite a while, and when it does, it will not be 3.0 anymore :-).

\warning In any case, THE PROPER WAY TO ACCESS THESE STRUCTURES IS THROUGH
THE RPM LIBRARY!!

The RPM file format covers both source and binary packages.  An RPM
package file is divided in 4 logical sections:

```
. Lead      -- 96 bytes of "magic" and other info
. Signature -- collection of "digital signatures"
. Header    -- holding area for all the package information (aka "metadata")
. Payload   -- compressed archive of the file(s) in the package (aka "payload")
```

All 2 and 4 byte "integer" quantities (int16 and int32) are stored in
network byte order.  When data is presented, the first number is the
byte number, or address, in hex, followed by the byte values in hex,
followed by character "translations" (where appropriate).

## Lead

The Lead is basically for file(1).  All the information contained in
the Lead is duplicated or superceded by information in the Header.
Much of the info in the Lead was used in old versions of RPM but is
now ignored.  The Lead is stored as a C structure:

\code
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
\endcode

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
architecture is stored as a string in the Header.  See, lib/misc.c for
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

The remaining 16 bytes (80-95) are currently unused and are reserved
for future expansion.

## Signature

A 3.0 format signature (denoted by signature type 5 in the Lead), uses
the same structure as the Header.  For historical reasons, this
structure is called a "header structure", which can be confusing since
it is used for both the Header and the Signature.  The details of the
header structure are given below, and you'll want to read them so the
rest of this makes sense.  The tags for the Signature are defined in
lib/signature.h.

The Signature can contain multiple signatures, of different types.
There are currently only three types, each with its own tag in the
header structure:

```
	Name	Tag	Header Type
	----	----	-----------
	SIZE	1000	INT_32
	MD5	1001	BIN
	PGP	1002	BIN
```

The MD5 signature is 16 bytes, and the PGP signature varies with
the size of the PGP key used to sign the package.

As of RPM 2.1, all packages carry at least SIZE and MD5 signatures,
and the Signature section is padded to a multiple of 8 bytes.

## Header

The Header contains all the information about a package: name,
version, file list, etc.  It uses the same "header structure" as the
Signature, which is described in detail below.  A complete list of the
tags for the Header would take too much space to list here, and the
list grows fairly frequently.  For the complete list see lib/rpmlib.h
in the RPM sources.

## Payload

The Payload is currently a gzipped cpio archive.  The cpio
archive type used is SVR4 with a CRC checksum.

## The Header Structure

The header structure is a little complicated, but actually performs a
very simple function.  It acts almost like a small database in that it
allows you to store and retrieve arbitrary data with a key called a
"tag".  When a header structure is written to disk, the data is
written in network byte order, and when it is read from disk, is is
converted to host byte order.

Along with the tag and the data, a data "type" is stored, which indicates,
obviously, the type of the data associated with the tag.  There are
currently 9 types:

```
	Type		Number
	----		------
	NULL		0
	CHAR		1
	INT8		2
	INT16		3
	INT32		4
	INT64		5
	STRING		6
	BIN		7
	STRING_ARRAY	8
	I18NSTRING_TYPE	9
```

One final piece of information is a "count" which is stored with each
tag, and indicates the number of items of the associated type that are
stored.  As a special case, the STRING type is not allowed to have a
count greater than 1.  To store more than one string you must use a
STRING_ARRAY.

Altogether, the tag, type, count, and data are called an "Entry" or
"Header Entry".

```
00000000: 8e ad e8 01 00 00 00 00    ........
```

A header begins with 3 bytes of magic "8e ad e8" and a single byte to
indicate the header version.  The next four bytes (4-7) are reserved.

```
00000008: 00 00 00 20 00 00 07 77    ........
```

The next four bytes (8-11) form an int32 that is a count of the number
of entries stored (in this case, 32).  Bytes 12-15 form an int32 that
is a count of the number of bytes of data stored (that is, the number
of bytes made up by the data portion of each entry).  In this case it
is 1911 bytes.

```
00000010: 00 00 03 e8 00 00 00 06 00 00 00 00 00 00 00 01    ................
```

Following the first 16 bytes is the part of the header called the
"index".  The index is made of up "index entries", one for each entry
in the header.  Each index entry contains four int32 quantities.  In
order, they are: tag, type, offset, count.  In the above example, we
have tag=1000, type=6, offset=0, count=1.  By looking up the the tag
in lib/rpmlib.h we can see that this entry is for the package name.
The type of the entry is a STRING.  The offset is an offset from the
start of the data part of the header to the data associated with this
entry.  The count indicates that there is only one string associated
with the entry (which we really already knew since STRING types are
not allowed to have a count greater than 1).

In our example there would be 32 such 16-byte index entries, followed
by the data section:

```
00000210: 72 70 6d 00 32 2e 31 2e 32 00 31 00 52 65 64 20    rpm.2.1.2.1.Red 
00000220: 48 61 74 20 50 61 63 6b 61 67 65 20 4d 61 6e 61    Hat Package Mana
00000230: 67 65 72 00 31 e7 cb b4 73 63 68 72 6f 65 64 65    ger.1...schroede
00000240: 72 2e 72 65 64 68 61 74 2e 63 6f 6d 00 00 00 00    r.redhat.com....
...
00000970: 6c 69 62 63 2e 73 6f 2e 35 00 6c 69 62 64 62 2e    libc.so.5.libdb.
00000980: 73 6f 2e 32 00 00                                  so.2..
```

The data section begins at byte 528 (4 magic, 4 reserved, 4 index
entry count, 4 data byte count, 16 * 32 index entries).  At offset 0,
bytes 528-531 are "rpm" plus a null byte, which is the data for the
first index entry (the package name).  Following is is the data for
each of the other entries.  Each string is null terminated, the strings
in a STRING_ARRAY are also null terminated and are place one after
another.  The integer types are aligned to appropriate byte boundaries,
so that the data of INT64 type starts on an 8 byte boundary, INT32
type starts on a 4 byte boundary, and an INT16 type starts on a 2 byte
boundary.  For example:

```
00000060: 00 00 03 ef 00 00 00 06 00 00 00 28 00 00 00 01    ................
00000070: 00 00 03 f1 00 00 00 04 00 00 00 40 00 00 00 01    ................
...
00000240: 72 2e 72 65 64 68 61 74 2e 63 6f 6d 00 00 00 00    r.redhat.com....
00000250: 00 09 9b 31 52 65 64 20 48 61 74 20 4c 69 6e 75    ....Red Hat Linu
```

Index entry number 6 is the BUILDHOST, of type STRING.  Index entry
number 7 is the SIZE, of type INT32.  The corresponding data for entry
6 end at byte 588 with "....redhat.com\0".  The next piece of data
could start at byte 589, byte that is an improper boundary for an INT32.
As a result, 3 null bytes are inserted and the date for the SIZE actually
starts at byte 592: "00 09 9b 31", which is 629553).

## Tools

The tools directory in the RPM sources contains a number of small
programs that use the RPM library to pick apart packages.  These
tools are mostly used for debugging, but can also be used to help
you understand the internals of the RPM package format.

```
	rpmlead		- extracts the Lead from a package
	rpmsignature	- extracts the Signature from a package
	rpmheader	- extracts the Header from a package
	rpmarchive	- extracts the Archive from a package
	dump		- displays a header structure in readable format
```

Given a package foo.rpm you might try:

```
	rpmlead foo.rpm | od -x
	rpmsignature foo.rpm | dump
	rpmheader foo.rpm | dump
	rpmarchive foo.rpm | zcat | cpio --list
```
