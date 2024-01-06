---
layout: default
title: rpm.org - Signatures and Digests
---

# Signatures and Digests

Table describing signatures and digests which RPM uses to verify package
contents:

|   RPMSIGTAG_   |      RPMTAG_      | Version | Deprecated |    Algorithm    | Location |  Range  |
|     :---:      |    :-------:      |  :---:  |   :---:    |     :-----:     |   :--:   | :-----: |
|       MD5      |     SIGMD5        |   3.0   |     Y      | MD5             |    S     |   HP    |
|       PGP      |     SIGPGP        |   3.0   |     Y      | OpenPGP/RSA     |    S     |   HP    |
|       GPG      |     SIGGPG        |   3.0   |     Y      | OpenPGP/DSA     |    S     |   HP    |
|       SHA1     |   SHA1HEADER      |   4.0   |     Y      | SHA1            |    S     |   H     |
|       RSA      |    RSAHEADER      |   4.0   |            | OpenPGP/RSA     |    S     |   H     |
|       DSA      |    DSAHEADER      |   4.0   |            | OpenPGP/(Ed)DSA |    S     |   H     |
|      SHA256    |  SHA256HEADER     |  4.14   |            | SHA256          |    S     |   H     |
|        -       |     FILEMD5       |   3.0   |     Y      | MD5             |    H     |   F     |
|        -       |   FILEDIGESTS     |   4.6   |            | SHA256 (**)     |    H     |   F     |
|        -       |  PAYLOADDIGEST    |  4.14   |            | SHA256 (*)      |    H     |   Pc    |
|        -       |  PAYLOADDIGESTALT |  4.16   |            | SHA256 (*)      |    H     |   P     |

* S = Signature header
* H = Main header
* P = Payload
* F = Files in the payload (uncompressed)
* c = compressed content
* (*) = Configurable, defaults to SHA256
* (**) = Configurable, defaults to SHA256 in rpm >= 4.14, MD5 in older
