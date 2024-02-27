---
date: 27 January 2020
section: 8
title: RPM2ARCHIVE
---

NAME
====

rpm2archive - Create tar archive from RPM Package Manager (RPM) package.

SYNOPSIS
========

**rpm2archive** **{-n\|\--nocompression}** **{-f\|\--format=pax|cpio}** *FILES*

DESCRIPTION
===========

**rpm2archive** converts the .rpm files specified as arguments to tar archives
on standard out.

If a \'-\' argument is given, an rpm stream is read from standard in.

If standard out is connected to a terminal, the output is written to tar files
with a \".tgz\" suffix, gzip compressed by default.

In opposite to **rpm2cpio** **rpm2archive** also works with RPM packages
containing files greater than 4GB which are not supported by cpio.

OPTIONS
=======

**-n, \--nocompression**

:   Generate uncompressed tar archive and use \".tar\" as postfix of the
    file name.

**-f, \--format=pax\|cpio**

:   Generate archive in specified format: **pax** (the default format)
    or **cpio**. Note that the cpio format cannot host files over
    4GB in size and is only supported here for backwards compatibility.

EXAMPLES
========

\
***rpm2archive glint-1.0-1.i386.rpm \| tar -xvz***\
***rpm2archive glint-1.0-1.i386.rpm ; tar -xvz glint-1.0-1.i386.rpm.tgz***\
***cat glint-1.0-1.i386.rpm \| rpm2archive - \| tar -tvz***

SEE ALSO
========

**rpm2cpio**(8), **rpm**(8)

AUTHOR
======

    Florian Festi <ffesti@redhat.com>
