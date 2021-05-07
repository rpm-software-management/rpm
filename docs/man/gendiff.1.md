---
date: Mon Jan 10 2000
section: 1
title: GENDIFF
---

NAME
====

gendiff - utility to aid in error-free diff file generation

SYNOPSIS
========

**gendiff** \<directory\> \<diff-extension\>

DESCRIPTION
===========

**gendiff** is a rather simple script which aids in generating a diff
file from a single directory. It takes a directory name and a
\"diff-extension\" as its only arguments. The diff extension should be a
unique sequence of characters added to the end of all original,
unmodified files. The output of the program is a diff file which may be
applied with the **patch** program to recreate the changes.

The usual sequence of events for creating a diff is to create two
identical directories, make changes in one directory, and then use the
**diff** utility to create a list of differences between the two. Using
gendiff eliminates the need for the extra, original and unmodified
directory copy. Instead, only the individual files that are modified
need to be saved.

Before editing a file, copy the file, appending the extension you have
chosen to the filename. I.e. if you were going to edit somefile.cpp and
have chosen the extension \"fix\", copy it to somefile.cpp.fix before
editing it. Then edit the first copy (somefile.cpp).

After editing all the files you need to edit in this fashion, enter the
directory one level above where your source code resides, and then type

        $ gendiff somedirectory .fix > mydiff-fix.patch

You should redirect the output to a file (as illustrated) unless you
want to see the results on stdout.

SEE ALSO
========

**diff**(1), **patch**(1)

AUTHOR
======

    Marc Ewing <marc@redhat.com>
