---
layout: default
title: rpm.org - RPM V6 Package format
---
# V6 Package format DRAFT

This document describes the RPM file format version 6, which is used
by RPM versions 6.x and with limitations, readable with 4.x.

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
All padding must consist solely of zero valued bytes.

## Lead

The Lead is used for identifying RPM package files based on the "magic"
value. The rest of the data in the Lead is historical only and is not
used by RPM.

The Lead is always 96 bytes long and starts with a four byte "magic"
[ 0xED, 0xAB, 0xEE, 0xDB ]. For v6 packages, the "version" field contains
4 for backwards compatibility reasons, and OS and architecture are zeros.
For further details, refer to the [Lead Format](format_lead.md) document.

## Signature

The Signature uses the same underlying [data structure](format_header.md)
as the Header. It's size is internally padded to a multiple of 8 bytes.

The Signature consists of a single immutable header region denoted
by it's first tag of 62 (RPMTAG_HEADERSIGNATURES) which can be used
to identify it.

The Signature can contain several tags of different types:

Name        	    | Tag   | Header Type
--------------------|-------|------------
HEADERSIGNATURES    |   62  | BIN
DSA                 |  267  | BIN
RSA                 |  268  | BIN
SHA256              |  272  | STRING
FILESIGNATURES      |  274  | STRING_ARRAY
VERITYSIGNATURES    |  276  | STRING_ARRAY
VERITYSIGNATUREALGO |  277  | INT_32
RESERVED            |  999  | BIN

All packages carry at least HEADERSIGNATURES, SHA256 and RESERVED tags.

On digitally signed packages, one of RSA or DSA tags is present and
contains an OpenPGP signature on the Header. The RSA tag is used for
RSA signatures and the DSA tag is used for EcDSA signatures.

Tags numbers above 999, including those of v3 (header+payload) signatures,
are considered illegal on v6 packages.

In addition, a package may also have either IMA or fsverity signatures
on it's files. If present, these are in FILESIGNATURES and VERITYSIGNATURES
tags respectively.

RESERVED is always the last tag in the Signature. It's used as a space
reservation for signatures and consists solely of zeros.

Strict tag sorting is required, and no duplicate tags are permitted.

In summary, the difference to v4 Signature is that obsolete cryptography
has been removed along with all size and payload relevant data, and space
reservation uses a different tag.  Also, there are no tag number conflicts
between the Signature and the Header.

## Header

The Header contains all the information about a package: name,
version, file list, etc.  It uses the same underlying
[data structure](format_header.md) as the Signature.

The Header consists of a single immutable header region denoted
by it's first tag of 63 (RPMTAG_HEADERIMMUTABLE) which can be used
to identify it.

Strict tag sorting is required, and no duplicate tags are permitted.

The complete list of tags is documented [here](tags.md), but in particular
for v6:
- New numeric tag RPMTAG_RPMFORMAT for the rpm package format version
- RPMTAG_ENCODING is required to be present and contain "utf-8"
- RPMTAG_LONGFILESIZES are always used to represent file sizes
- RPMTAG_FILEDIGESTALGO is always present and at least SHA256 in strength
- RPMTAG_PAYLOADDIGEST and RPMTAG_PAYLOADDIGESTALT are always present
  and contain SHA256 hashes of the Payload (compressed and
  uncompressed). Thus a header signature is sufficient to establish
  cryptographic provenance of the package, without having to separately
  calculate the payload signature. The alternatives allow freely switching
  between a compressed and uncompressed payload for a package.
- Similarly, there are two alternative size tags on the Payload (compressed
  and uncompressed): RPMTAG_PAYLOADSIZE and RPMTAG_PAYLOADSIZEALT
- File type information is stored as MIME types instead of libmagic
  strings in RPMTAG_MIMEDICT and RPMTAG_FILEMIMEINDEX, retrievable
  with RPMTAG_FILEMIMES extension.

## Payload

The payload is a stripped-down version of cpio, using `07070X` as magic bytes.
The file header only contains the index number of the file in the RPM
header as an 8 byte hex string. The payload may be compressed.

Note: This is the format v4 uses for handling > 4GB files.

## Differences to V4

The main differences of the V4 package format are:
- Cryptographic algorithms have been updated to contemporary standards
- Signature only carries cryptographic content
- Signature tags no longer clash with other tags
- All sizes are stored as 64bit integers (ie the LONG-variants size tags)
- Payload is verifiable independently of the other package components
- Payload is always in the "new" rpm specific format which has no size limits
- Strings are always UTF-8 encoded

