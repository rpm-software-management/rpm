---
date: 29 June 2010
section: 8
title: RPMDB
---

NAME
====

rpmdb - RPM Database Tool

SYNOPSIS
========

**rpmdb** {**\--initdb\|\--rebuilddb**}

**rpmdb** {**\--verifydb**}

**rpmdb** {**\--exportdb\|\--importdb**}

DESCRIPTION
===========

The general form of an rpmdb command is

**rpm** {**\--initdb\|\--rebuilddb**} \[**-v**\] \[**\--dbpath
***DIRECTORY*\] \[**\--root ***DIRECTORY*\]

Use **\--initdb** to create a new database if one doesn\'t already exist
(existing database is not overwritten), use **\--rebuilddb** to rebuild
the database indices from the installed package headers.

**\--verifydb** performs a low-level integrity check on the database.

**\--exportdb** exports the database in header-list format, suitable
for transfporting to another host or database type.

**\--importdb** imports a database from a header-list format as created
by **\--exportdb**.

SEE ALSO
========

**popt**(3), **rpm**(8), **rpmkeys**(8), **rpmsign**(8), **rpm2cpio**(8),
**rpmbuild**(8), **rpmspec**(8)

**rpm \--help** - as rpm supports customizing the options via popt
aliases it\'s impossible to guarantee that what\'s described in the
manual matches what\'s available.

**http://www.rpm.org/ \<URL:http://www.rpm.org/\>**

AUTHORS
=======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
    Panu Matilainen <pmatilai@redhat.com>
