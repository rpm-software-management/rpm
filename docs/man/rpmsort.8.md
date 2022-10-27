---
date: 27 October 2022
section: 8
title: RPMSORT
---

NAME
====

rpmsort - Sort input by RPM Package Manager (RPM) versioning.

SYNOPSIS
========

**rpmsort** *FILES*

DESCRIPTION
===========

**rpmsort** sorts the input files, and writes a sorted list to standard out -
like sort(1), but aware of RPM versioning.

If \'-\' is given as an argument, or no arguments are given, versions are read
from stdandard in and writen to standard out.

EXAMPLES
========

\
***$ echo -e 'rpm-4.18.0-3.fc38.x86_64\nrpm-4.18.0-1.fc38.x86_64' | rpmsort \
rpm-4.18.0-1.fc38.x86_64 \
rpm-4.18.0-3.fc38.x86_64***

AUTHORS
=======

    Peter Jones <pjones@redhat.com>
	Robbie Harwood <rharwood@redhat.com>
