---
date: 28 Jan 2020
section: 8
title: 'RPM-AUDIT'
---

NAME
====

rpm-plugin-audit - Audit plugin for the RPM Package Manager

Description
===========

The plugin writes basic information about rpm transactions to the audit
log - like packages installed or removed. The entries can be viewed with

**ausearch -m SOFTWARE\_UPDATE**

Data fields
-----------

The entries in the audit log have the following fields:

**Field**

:   **Possible values Description**

```{=html}
<!-- -->
```

**op**

:   install/update/remove package operation

```{=html}
<!-- -->
```

**sw**

:   name-version-release.arch of the package

**key\_enforce**

:   0/1 are signatures being enforced

**gpg\_res**

:   0/1 result of signature check (0 == fail / 1 ==success)

**root\_dir**

:   Root directory of the operation, normally \"/\"

**sw\_type**

:   \"rpm\" package format

Configuration
=============

There are currently no options for this plugin in particular. See
**rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*ausearch*(8) *rpm-plugins*(8)
