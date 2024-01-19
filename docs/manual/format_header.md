---
layout: default
title: rpm.org - RPM Package Header format
---

## The Header Structure

The header structure is a generic key-value data store, supporting multiple
data types and accessed via integer keys known as tags.

The on-disk format of a header consists of three consequtive sections:
- Intro
- Index
- Data

All integer values is stored in network byte order (big-endian) on disk.

**THE PROPER WAY TO ACCESS THESE STRUCTURES IS THROUGH THE RPM LIBRARY!!**

### Intro

The header intro can be defined as follows:
```
struct header_intro {
    unsigned char magic[8];
    uint32_t index_length;
    uint32_t data_length;
};
```

The header magic value is used to indentify the header, and is
0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00.

The index length contains the number of Index Entries, and the data_length
is the size of the Data section in bytes.

### Index

The Index is the means to find the data stored in the header.
It consists of Index Entries which can be defined as follows:
```
struct index_entry {
    uint32_t tag;
    uint32_t type;
    int32_t offset;
    uint32_t count;
};
```

The Index Entries are sorted by the tag number.

The tag is the identifier key for the data starting at the given offset in
the Data section. The following data types are supported:

Type            | Number| Type size
----------------|-------|----------
CHAR    		| 1     | 1
INT8    		| 2     | 1
INT16   		| 3     | 2
INT32   		| 4     | 4
INT64   		| 5     | 8
STRING		    | 6     | -
BIN		        | 7     | 1
STRING_ARRAY    | 8     | -
I18NSTRING_TYPE	| 9     | -

In addition, each entry has an associated count which denotes the number
of values in this entry. BIN is effectively the same as CHAR, but denotes
a contiguous binary blob rather than an array of byte-sized values.
As a special case, STRING can only have count of one.

## Data

The integer types are aligned to appropriate byte boundaries,
so that the data of INT64 type starts on an 8 byte boundary, INT32
type starts on a 4 byte boundary, and an INT16 type starts on a 2 byte
boundary.

Each string is null terminated, the strings in a STRING_ARRAY are also
null-terminated and are place one after another.

The size of integral types is a straightforward multiplication based
on the type size in the above table, but the string sizes must be
determined by walking the data looking for the null-bytes while taking
care to stay within bounds.

## A practical example

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

## Immutable regions

RPM v4 introduced the concept of contiguous immutable header regions
which allow the original header data to be digitally verified even after
modifying the data. This is done with special tags which keep track
of a contiguous region (i.e. the original header).

These region tags are technically like any other tag with associated
binary data and thus fully backwards compatible. The special part is the
interpretation of the region tag data, called the trailer, which looks
like a Index Entry despite residing in the Data section,

A region Index Entry looks like this:

Field   | Value
--------|------
tag     | 62 or 63 (HEADERIMMUTABLE or HEADERSIGNATURES)
type    | BIN
offset  | Offset to the region trailer in the Data section
count   | 16

And the region trailer in the Data section:

Field   | Value
--------|------
tag     | Must equal the Index Entry (ie 62 or 63)
type    | BIN
offset  | Size of the region entries in the Index in bytes
count   | 16

The number of entries in the region (aka region index length) can thus be
calculated as `ril = -offset / sizeof(struct index_entry)`.

When reading a package from disk, the number of region entries is expected
to be the same as the index length in the Intro. However when a package
is installed, extra data such as the install time is added to the header,
that data falls outside the otherwise invisible region line in the index.
These tags outside the immutable region are called "dribbles" in the RPM
lore.

With the aid of regions and dribbles, it's possible to add, modify and
delete header data but still pull out the original contents at will.
It gets complicated.
