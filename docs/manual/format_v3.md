---
layout: default
title: rpm.org - RPM V3 Package format
---
# V3 Package format

This document describes the RPM file format version 3, which is used
by RPM versions 2.1 to 3.x and still installable by 4.x.

**THE PROPER WAY TO ACCESS THESE STRUCTURES IS THROUGH THE RPM LIBRARY!!**

The RPM file format covers both source and binary packages.  An RPM
package file is divided in 4 logical sections:

```
. Lead      -- 96 bytes of "magic" and other info
. Signature -- collection of "digital signatures"
. Header    -- holding area for all the package information (aka "metadata")
. Payload   -- compressed archive of the file(s) in the package (aka "payload")
```

All applicaple integer quantities are stored in network byte order
(big-endian). When data is presented, the first number is the
byte number, or address, in hex, followed by the byte values in hex,
followed by character "translations" (where appropriate).

## Lead

The Lead is used for identifying RPM package files.
The rest of the data in the Lead is historical only.

The Lead is always 96 bytes long and starts with a four byte "magic"
[ 0xED, 0xAB, 0xEE, 0xDB ]. For further details, refer to the
[Lead Format](format_lead.md) document.


## Signature

The Signature uses the same underlying [data structure](format_header.md)
as the Header, but is zero-padded to a multiple of 8 bytes.

The Signature can contain several tags of different types:

Name	| Tag   | Header Type
--------|-------|----
SIZE	| 1000	| INT_32
PGP     | 1002	| BIN
MD5     | 1004	| BIN
GPG     | 1005  | BIN

All packages carry at least SIZE and MD5 tags. The MD5 binary hash
is 16 bytes long.

On digitally signed packages, one of PGP or GPG tags is present and
contains an OpenPGP signature on the header + payload data. The PGP
tag is used for RSA signatures and the GPG tag is used for DSA
signatures.

Note: the signature tag numbers clash with those of the main header,
care must be taken not to mix them up.

## Header

The Header contains all the information about a package: name,
version, file list, etc.  It uses the same underlying
[data structure](format_header.md) as the Signature.
The complete list of tags is documented [here](tags.md).

## Payload

The Payload is currently a cpio archive, gzipped by default.  The cpio archive
type used is SVR4 with a CRC checksum.

