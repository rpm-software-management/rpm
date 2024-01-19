---
layout: default
title: rpm.org - RPM V3 Package format
---
# V3 Package format

This document describes the RPM file format version 3.0, which is used
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

A V3 Signature uses the same underlying [data structure](format_header.md)
as the Header, but is zero-padded to a multiple of 8 bytes.

The Signature can contain multiple signatures, of different types.
There are currently only three types, each with its own tag in the
header structure:

Name	| Tag   | Header Type
--------|-------|----
SIZE	| 1000	| INT_32
MD5     | 1001	| BIN
PGP     | 1002	| BIN

The MD5 signature is 16 bytes, and the PGP signature varies with
the size of thekey used to sign the package. 

All packages carry at least SIZE and MD5 signatures.
The PGP tag is present in digitally signed packages and contains an
OpenPGP RSA signature on the header + payload data.

## Header

The Header contains all the information about a package: name,
version, file list, etc.  It uses the same underlying
[data structure](format_header.md) as the Signature.
The complete list of tags is documented [here](tags.md).

## Payload

The Payload is currently a cpio archive, gzipped by default.  The cpio archive
type used is SVR4 with a CRC checksum.

As cpio is limited to 4 GB (32 bit unsigned) file sizes RPM since
version 4.12 uses a stripped down version of cpio for packages with
files > 4 GB. This format uses `07070X` as magic bytes and the file
header otherwise only contains the index number of the file in the RPM
header as 8 byte hex string. The file metadata that is normally found
in a cpio file header - including the file name - is completely
omitted as it is stored in the RPM header already.

To use a different compression method when building new packages with
`rpmbuild(8)`, define the `%_binary_payload` or `%_source_payload` macros for
the binary or source packages, respectively.  These macros accept an
[RPM IO mode string](https://ftp.osuosl.org/pub/rpm/api/4.17.0/group__rpmio.html#example-mode-strings)
(only `w` mode).

