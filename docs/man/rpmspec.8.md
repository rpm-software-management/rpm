---
date: 29 October 2010
section: 8
title: RPMSPEC
---

NAME
====

rpmspec - RPM Spec Tool

SYNOPSIS
========

QUERYING SPEC FILES:
--------------------

**rpmspec** {**-q\|\--query**} \[**select-options**\]
\[**query-options**\] *SPEC\_FILE \...*

PARSING SPEC FILES TO STDOUT:
-----------------------------

**rpmspec** {**-P\|\--parse**} *SPEC\_FILE \...*

DESCRIPTION
===========

**rpmspec** is a tool for querying a spec file. More specifically for
querying hypothetical packages which would be created from the given
spec file. So querying a spec file with **rpmspec** is similar to
querying a package built from that spec file. But is is not identical.
With **rpmspec** you can\'t query all fields which you can query from a
built package. E. g. you can\'t query BUILDTIME with **rpmspec** for
obvious reasons. You also cannot query other fields automatically
generated during a build of a package like auto generated dependencies.

select-options
--------------

\[**\--rpms**\] \[**\--srpm**\]

query-options
-------------

\[**\--qf,\--queryformat ***QUERYFMT*\] \[**\--target
***TARGET\_PLATFORM*\]

QUERY OPTIONS
-------------

The general form of an rpm spec query command is

**rpm** {**-q\|\--query**} \[**select-options**\] \[**query-options**\]

You may specify the format that the information should be printed in. To
do this, you use the

**\--qf\|\--queryformat** *QUERYFMT*

option, followed by the *QUERYFMT* format string. See **rpm(8)** for
details.

SELECT OPTIONS
--------------

**\--rpms** Operate on the all binary package headers generated from
spec. **\--builtrpms** Operate only on the binary package headers of
packages which would be built from spec. That means ignoring package
headers of packages that won\'t be built from spec i. e. ignoring
package headers of packages without file section. **\--srpm** Operate on
the source package header(s) generated from spec.

EXAMPLES
========

Get list of binary packages which would be generated from the rpm spec
file:

>      $ rpmspec -q rpm.spec
>      rpm-4.11.3-3.fc20.x86_64
>      rpm-libs-4.11.3-3.fc20.x86_64
>      rpm-build-libs-4.11.3-3.fc20.x86_64
>      ...
>
>     Get summary infos for single binary packages generated from the rpm spec file:
>
>      $ rpmspec -q --qf "%{name}: %{summary}\n" rpm.spec
>      rpm: The RPM package management system
>      rpm-libs: Libraries for manipulating RPM packages
>      rpm-build-libs: Libraries for building and signing RPM packages
>      ...
>
>     Get the source package which would be generated from the rpm spec file:
>
>      $ rpmspec -q --srpm rpm.spec
>      rpm-4.11.3-3.fc20.x86_64
>
>     Parse the rpm spec file to stdout:
>
>      $ rpmspec -P rpm.spec
>      Summary: The RPM package management system
>      Name: rpm
>      Version: 4.14.0
>      ...

SEE ALSO
========

    popt(3),
    rpm(8),
    rpmdb(8),
    rpmkeys(8),
    rpmsign(8),
    rpm2cpio(8),
    rpmbuild(8),

**rpmspec \--help** - as rpm supports customizing the options via popt
aliases it\'s impossible to guarantee that what\'s described in the
manual matches what\'s available.

**http://www.rpm.org/ \<URL:http://www.rpm.org/\>**

AUTHORS
=======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
    Panu Matilainen <pmatilai@redhat.com>
