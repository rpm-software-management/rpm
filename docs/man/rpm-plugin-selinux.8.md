---
date: 14 Apr 2016
section: 8
title: 'RPM-SELINUX'
---

NAME
====

rpm-plugin-selinux - SELinux plugin for the RPM Package Manager

Description
===========

The plugin sets SELinux contexts for installed files and executed
scriptlets. It needs SELinux to be enabled to work but will work in both
enforcing and permissive mode.

Configuration
=============

The plugin can be disabled temporarily by passing **\--nocontexts** at
the RPM command line or setting the transaction flag
**RPMTRANS\_FLAG\_NOCONTEXTS** in the API.

See **rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*rpm*(8) *rpm-plugins*(8)
