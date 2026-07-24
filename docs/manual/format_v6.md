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

All applicable integer quantities are stored in network byte order
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
DSA (legacy)        |  267  | BIN
RSA (legacy)        |  268  | BIN
SHA256              |  272  | STRING
FILESIGNATURES      |  274  | STRING_ARRAY
VERITYSIGNATURES    |  276  | STRING_ARRAY
VERITYSIGNATUREALGO |  277  | INT_32
OPENPGP             |  278  | STRING_ARRAY
SHA3_256            |  279  | STRING
RESERVED            |  999  | BIN

All packages carry at least HEADERSIGNATURES, SHA256, SHA3_256 and
RESERVED tags.

On digitally signed packages, the OPENPGP tag will contain one or more
base64-encoded OpenPGP signatures on the Header. The number of signatures
per package is unlimited except for the limits presented by the containing
header. The signatures in the OPENPGP tag are known as RPM v6 signatures.

In addition, a v6 package with an RPM v6 signature may also have one
one of the RSA or DSA tags present, known as RPM v4 signatures.
These exist solely for the purpose of RPM 4.x compatibility, RPM will ignore
v4 signature tags if v6 signatures are present.

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
- New string tag RPMTAG_SOURCENEVR in binary packages, identifying the
  name-[epoch:]-version-release of the package source
- RPMTAG_ENCODING is required to be present and contain "utf-8"
- RPMTAG_LONGFILESIZES are always used to represent file sizes
- RPMTAG_FILEDIGESTALGO is always present and at least SHA256 in strength
- RPMTAG_PAYLOADSHA256 and RPMTAG_PAYLOADSHA256ALT (optional in v4),
  containing SHA256 hashes of the Payload (compressed and uncompressed).
- New RPMTAG_PAYLOADSHA512, RPMTAG_PAYLOADSHA512ALT,
- RPMTAG_PAYLOADSHA3_256 and RPMTAG_PAYLOADSHA3_256ALT tags, containing
  SHA512 and SHA3-256 hashes of the Payload (compressed and uncompressed).
- Similarly, there are two alternative size tags on the Payload (compressed
  and uncompressed): RPMTAG_PAYLOADSIZE and RPMTAG_PAYLOADSIZEALT
- File type information is stored as MIME types instead of libmagic
  strings in RPMTAG_MIMEDICT and RPMTAG_FILEMIMEINDEX, retrievable
  with RPMTAG_FILEMIMES extension.
- The optional RPMTAG_PAYLOADALIGNMENT records the payload content alignment,
  see the Payload section.

The Payload hashes in a signed Header are sufficient to establish
cryptographic provenance of the package, without having to separately
calculate the payload signature. The alternatives allow freely switching
between a compressed and uncompressed payload for a package.

## Payload

The payload is an RPM-specific extension of cpio, using `07070X` as magic
bytes. The file header only contains the index number of the file in the RPM
header as an 8 byte hex string. The payload may be compressed. Features such
as stripped headers and content alignment mean it must be decoded by an
RPM-aware reader rather than treated as a generic cpio stream.

Note: This is the format v4 uses for handling > 4GB files.

### Aligned payloads

When RPMTAG_PAYLOADALIGNMENT is present and non-zero, the payload is laid out
so that regular file content starts on an alignment boundary:

- The tag value is the alignment in bytes, a power of two, normally the
  filesystem block size (4096), with a supported maximum of 1 MiB.
- For an uncompressed payload the canonical cpio stream starts on an alignment
  boundary. The zero framing between the Header and cpio is covered by the
  primary physical payload digests and size, but excluded from the canonical
  uncompressed (ALT) payload digests and size.
- Within the payload, the content of every regular file at least as large as
  the alignment starts on an alignment boundary. Smaller files, symlinks,
  directories and other members keep the ordinary 4-byte cpio alignment.

The additional per-member padding is an RPM cpio extension. Readers that are
unaware of RPMTAG_PAYLOADALIGNMENT cannot process an aligned payload as a
generic cpio archive; conversion tools must reconstruct a canonical archive.
Aligned packages require `rpmlib(PayloadAlignment) <= 6.2.0-1` to keep older
RPM versions from attempting to install an unsupported payload layout.

The per-member zero padding is inside the payload compression, so it normally
costs only a small encoding overhead in transit.

A signature-preserving local materialization keeps the signed main header and
ALT digest unchanged, replaces the compressed bytes with canonical raw cpio,
and for an aligned package adds the outer zero framing only on disk.
Header-only signatures remain valid; signature-header records covering the old
physical payload are removed. The compressor and primary payload digest/size
tags in the signed main header intentionally continue to describe the original
transport representation, while RPM verifies the physical payload against the
ALT tags. A package built without content alignment can still be materialized,
but its file content lands on arbitrary offsets, so it can only be copied
from, not cloned. Re-alignment is not permitted because it would change the
signed canonical bytes.

Aligning the content this way lets tools clone regular file content into and
out of the package with `FICLONERANGE`, sharing extents on filesystems that
support reflinks (btrfs, XFS, ...). Where cloning is unavailable, RPM falls
back to `copy_file_range(2)`. Verified installs read back and hash the exact
bytes placed in the destination.

## Differences to V4

The main differences of the V4 package format are:
- Cryptographic algorithms have been updated to contemporary standards
- Signature only carries cryptographic content
- Signature tags no longer clash with other tags
- All sizes are stored as 64bit integers (ie the LONG-variants size tags)
- Payload is verifiable independently of the other package components
- Payload is always in the "new" rpm specific format which has no size limits
- Strings are always UTF-8 encoded
