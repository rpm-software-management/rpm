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

**rpm** {**\--initdb\|\--rebuilddb**}

DESCRIPTION
===========

The general form of an rpm rebuild database command is

**rpm** {**\--initdb\|\--rebuilddb**} \[**-v**\] \[**\--dbpath
***DIRECTORY*\] \[**\--root ***DIRECTORY*\]

Use **\--initdb** to create a new database if one doesn\'t already exist
(existing database is not overwritten), use **\--rebuilddb** to rebuild
the database indices from the installed package headers.

SEE ALSO
========

    popt(3),
    rpm(8),
    rpmkeys(8),
    rpmsign(8),
    rpm2cpio(8),
    rpmbuild(8),
    rpmspec(8),

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
