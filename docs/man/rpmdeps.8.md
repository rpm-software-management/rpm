---
date: 24 October 2002
section: 8
title: RPMDEPS
---

NAME
====

rpmdeps - Generate RPM Package Dependencies

SYNOPSIS
========

**rpmdeps** **{-P\|\--provides}** **{-R\|\--requires}**
**{\--rpmfcdebug}** *FILE \...*

DESCRIPTION
===========

**rpmdeps** generates package dependencies for the set of *FILE*
arguments. Each *FILE* argument is searched for Elf32/Elf64, script
interpreter, or per-script dependencies, and the dependencies are
printed to stdout.

OPTIONS
=======

**-P, \--provides**

:   Print the provides

**-R, \--requires**

:   Print the requires

**\--recommends**

:   Print the recommends

**\--suggests**

:   Print the suggests

**\--supplements**

:   Print the supplements

**\--enhances**

:   Print the enhances

**\--conflicts**

:   Print the conflicts

**\--obsoletes**

:   Print the obsoletes

**\--alldeps**

:   Print all the dependencies

SEE ALSO
========

**rpm**(8),

**rpmbuild**(8),

AUTHORS
=======

Jeff Johnson \<jbj\@redhat.com\>
