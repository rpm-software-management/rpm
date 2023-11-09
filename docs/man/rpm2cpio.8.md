---
date: 11 January 2001
section: 8
title: rpm2cpio
---

NAME
====

rpm2cpio - Extract cpio archive from RPM Package Manager (RPM) package.

SYNOPSIS
========

**rpm2cpio** \[filename\]

DESCRIPTION
===========

**rpm2cpio** converts the .rpm file specified as a single argument to a
cpio archive on standard out. If a \'-\' argument is given, an rpm
stream is read from standard in.

**Note:** the CPIO format cannot host individual files over 4GB in size,
and so this tool is considered obsolete.  Use **rpm2archive** instead.

\
***rpm2cpio glint-1.0-1.i386.rpm \| cpio -dium***\
***cat glint-1.0-1.i386.rpm \| rpm2cpio - \| cpio -tv***

SEE ALSO
========

**rpm**(8) **rpm2archive**(8)

AUTHOR
======

    Erik Troan <ewt@redhat.com>
