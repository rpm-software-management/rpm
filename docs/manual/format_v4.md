---
layout: default
title: rpm.org - RPM V4 Package format
---
# V4 Package format

This document describes the RPM file format version 4, which is used
by RPM versions 4.x and with limitations, readable with 3.x.

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

The Signature consists of a single immutable header region denoted
by it's first tag of 62 (RPMTAG_HEADERSIGNATURES) which can be used
to identify it.

The Signature can contain several tags of different types:

Name        	    | Tag   | Header Type
--------------------|-------|----
HEADERSIGNATURES    |   62  | BIN
DSA                 |  267  | BIN
RSA                 |  268  | BIN
SHA1                |  269  | STRING
LONGSIZE            |  270  | INT_64
LONGARCHIVESIZE     |  271  | INT_64
SHA256              |  272  | STRING
FILESIGNATURES      |  274  | STRING_ARRAY
FILESIGNATURELENGTH |  275  | INT_32
VERITYSIGNATURES    |  276  | STRING_ARRAY
VERITYSIGNATUREALGO |  277  | INT_32
SIZE	            | 1000	| INT_32
PGP                 | 1002	| BIN
MD5                 | 1004	| BIN
GPG                 | 1005  | BIN
RESERVEDSPACE       | 1008  | BIN

All packages carry at least HEADERSIGNATURES, (LONG)SIZE, MD5 and SHA1,
and since rpm >= 4.14, SHA256 tags.

The MD5 binary hash is 16 bytes long. Other binary tag sizes vary
depending on key parameters and such. As a special case, the
RESERVEDSPACE tag is used as a space reservation for signatures to
allow for much faster package signing.

The 64bit size tags are only used in packages 4GB or larger in size,
but otherwise follow the same behavior as their 32bit counterparts.

On digitally signed packages, one of RSA or DSA tags is present and
contains an OpenPGP signature on the header. The RSA tag is used for
RSA signatures and the DSA tag is used for both EcDSA and original DSA
signatures. Additionally, an RPM v3 signature with the same key on the
header+payload may be present. For these, the PGP tag is used for RSA
signatures and the GPG tag is used for both EcDSA and original DSA signatures.

In addition, a package may also have either IMA or fsverity signatures
on it's files. If present, these are in FILESIGNATURES and VERITYSIGNATURES
tags respectively.

Note: some of the tag numbers clash with those of the main header, care
must be taken not to mix them up.

## Header

The Header contains all the information about a package: name,
version, file list, etc.  It uses the same underlying
[data structure](format_header.md) as the Signature.

The Header consists of a single immutable header region denoted
by it's first tag of 63 (RPMTAG_HEADERIMMUTABLE) which can be used
to identify it.

The complete list of tags is documented [here](tags.md).

## Payload

The Payload is a cpio archive, gzipped by default.  The cpio archive
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

## Differences to V3

The main differences of the V4 package format are:
- The immutable header region in Signature and Header
- Header-only signatures and hashes on the immutable region
- File paths stored in "compressed" format (dirname, dirindex, basename)

Later versions extended the format in various ways that are not backwards
compatible with early v4 if used:
- Support for 64bit integers (rpm >= 4.6, unreadable by older)
- Support for packages over 4GB (rpm >= 4.6, unreadable by older)
- Support for individual files over 4GB (rpm >= 4.12, unreadable by older)
- Per-file signatures (rpm >= 4.13)
- Separate hash on the payload (rpm >= 4.14)
