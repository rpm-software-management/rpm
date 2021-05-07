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

**rpm2archive** **{-n\|\--nocompression}** *FILES*

DESCRIPTION
===========

**rpm2archive** converts the .rpm files specified as arguments to tar
files. By default they are gzip compressed and saved with postfix
\".tgz\".

If \'-\' is given as argument, an rpm stream is read from standard in
and written to standard out.

In opposite to **rpm2cpio** **rpm2archive** also works with RPM packages
containing files greater than 4GB which are not supported by cpio.
Unless **rpm2cpio** **rpm2archive** needs a working rpm installation
which limits its usefulness for some disaster recovery scenarios.

OPTIONS
=======

**-n, \--nocompression**

:   Generate uncompressed tar archive and use \".tar\" as postfix of the
    file name.

EXAMPLES
========

\
***rpm2archive glint-1.0-1.i386.rpm ; tar -xvz
glint-1.0-1.i386.rpm.tgz***\
***cat glint-1.0-1.i386.rpm \| rpm2archive - \| tar -tvz***

SEE ALSO
========

*rpm2cpio*(8) *rpm*(8)

AUTHOR
======

    Florian Festi <ffesti@redhat.com>
