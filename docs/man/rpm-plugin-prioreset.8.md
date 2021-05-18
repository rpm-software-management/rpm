---
date: 28 Jan 2020
section: 8
title: 'RPM-PRIORESET'
---

NAME
====

rpm-plugin-prioreset - Plugin for the RPM Package Manager to fix issues
with priorities of deamons on SysV init

Description
===========

In general scriptlets run with the same priority as rpm itself. However
on legacy SysV init systems, properties of the parent process can be
inherited by the actual daemons on restart. As a result daemons may end
up with unwanted nice or ionice values. This plugin resets the scriptlet
process priorities after forking, and can be used to counter that
effect. Should not be used with systemd because it\'s not needed there,
and the effect is counter-productive.

Configuration
=============

There are currently no options for this plugin in particular. See
**rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*rpm*(8) *rpm-plugins*(8)
