---
layout: default
title: rpm.org - RPM Package Header format
---

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

Type            | Number
----------------|-------
NULL    		| 0
CHAR    		| 1
INT8    		| 2
INT16   		| 3
INT32   		| 4
INT64   		| 5
STRING		    | 6
BIN		        | 7
STRING_ARRAY    | 8
I18NSTRING_TYPE	| 9

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
in `include/rpm/rpmtag.h` we can see that this entry is for the package name.
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

