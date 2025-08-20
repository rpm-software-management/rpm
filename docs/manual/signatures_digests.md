---
layout: default
title: rpm.org - Signatures and Digests
---
# Signatures and Digests

Table describing signatures and digests which RPM uses to verify package
contents:

|   RPMSIGTAG_   |      RPMTAG_      | Version | Algorithm      | Location |  Range  |
|     :---:      |    :-------:      |  :---:  |  :-----:       |   :--:   | :-----: |
|       MD5      |     SIGMD5        |   3.0   | MD5            |    S     |   HP    |
|       PGP      |     SIGPGP        |   3.0   | OpenPGP/RSA    |    S     |   HP    |      
|       GPG      |     SIGGPG        |   3.0   | OpenPGP/DSA    |    S     |   HP    |
|       SHA1     |   SHA1HEADER      |   4.0   | SHA1           |    S     |   H     |
|       RSA      |    RSAHEADER      |   4.0   | OpenPGP/RSA    |    S     |   H     |
|       DSA      |    DSAHEADER      |   4.0   | OpenPGP/DSA    |    S     |   H     |
|      SHA256    |  SHA256HEADER     |  4.14   | SHA256         |    S     |   H     |
|      OPENPGP   |  OPENPGPHEADER    |   6.0   | any (***)      |    S     |   H     |
|        -       |  PAYLOADSHA256    |  4.14   | SHA256 (*)     |    H     |   Pc    |
|        -       |  PAYLOADSHA256ALT |  4.16   | SHA256 (*)     |    H     |   P     |
|        -       |  PAYLOADSHA512    |  6.0    | SHA512      |    H     |   Pc    |
|        -       |  PAYLOADSHA512ALT |  6.0    | SHA512      |    H     |   P     |
|        -       |  PAYLOADSHA3_256    |  6.0   | SHA3_256      |    H     |   Pc    |
|        -       |  PAYLOADSHA3_256ALT |  6.0   | SHA3_256      |    H     |   P     |
|        -       |     FILEMD5       |   3.0   | MD5            |    H     |   F     |
|        -       |   FILEDIGESTS     |   4.6   | SHA256 (**)    |    H     |   F     |

* S = Signature header
* H = Main header
* P = Payload
* F = Files in the payload (uncompressed)
* c = compressed content
* (*) = Known as PAYLOADDIGEST in rpm 4.x
* (**) = Configurable, defaults to SHA256 in rpm >= 4.14, MD5 in older
* (***) = Depends on the signing key, anything allowed by the OpenPGP standard
