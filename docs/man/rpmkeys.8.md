---
date: 29 October 2010
section: 8
title: RPMKEYS
---

NAME
====

rpmkeys - RPM Keyring

SYNOPSIS
========

**rpmkeys** {**\--list\|\--import\|\--delete\|\--checksig**}

DESCRIPTION
===========

The general forms of rpm digital signature commands are

**rpmkeys** **\--list** \[*FINGERPRINT \...*\]

**rpmkeys** **\--export** \[*FINGERPRINT \...*\]

**rpmkeys** **\--import** *PUBKEY \...*

**rpmkeys** **\--delete** *FINGERPRINT \...*

**rpmkeys** {**-K\|\--checksig**} *PACKAGE\_FILE \...*

The **\--checksig** option checks all the digests and signatures
contained in *PACKAGE\_FILE* to ensure the integrity and origin of the
package. Note that signatures are now verified whenever a package is
read, and **\--checksig** is useful to verify all of the digests and
signatures associated with a package.

Digital signatures cannot be verified without a public key. An ASCII
armored public key can be added to the **rpm** persistent keyring using
**\--import**.

The following commands are available for manipulating the persistent
rpm keyring:

**rpmkeys** **\--list** \[*FINGERPRINT \...*\]

List currently imported public key(s) (aka certificates) by their
fingerprint and user ID. If no fingerprints are specified, list all keys.

The fingerprint is the handle used for all operations on the keys.

**rpmkeys** **\--export** \[*FINGERPRINT \...*\]

Output the key(s) using an ASCII-armor encoding.

Exporting allows for inspecting the data with specialized tools, such
as Sequoia or GnuPG. For example:

**rpmkeys --export 771b18d3d7baa28734333c424344591e1964c5fc | sq inspect **

**rpmkeys** **\--delete** *FINGERPRINT \...*

Delete the key(s) designated by *FINGERPRINT*. For example:

**rpmkeys** **\--delete 771b18d3d7baa28734333c424344591e1964c5fc**

SEE ALSO
========

**popt**(3), **rpm**(8), **rpmdb**(8), **rpmsign**(8), **rpm2cpio**(8),
**rpmbuild**(8), **rpmspec**(8)

**rpmkeys \--help** - as rpm supports customizing the options via popt
aliases it\'s impossible to guarantee that what\'s described in the
manual matches what\'s available.

**http://www.rpm.org/ \<URL:http://www.rpm.org/\>**

AUTHORS
=======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
    Panu Matilainen <pmatilai@redhat.com>
