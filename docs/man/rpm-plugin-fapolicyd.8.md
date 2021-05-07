---
date: 28 Jan 2021
section: 8
title: 'RPM-FAPOLICYD'
---

NAME
====

rpm-plugin-fapolicyd - Fapolicyd plugin for the RPM Package Manager

Description
===========

The plugin gathers metadata of currently installed files. It sends the
information about files and about ongoing rpm transaction to the
fapolicyd daemon. The information is written to Linux pipe which is
placed in /var/run/fapolicyd/fapolicyd.fifo.

Configuration
=============

There are currently no options for this plugin in particular. See
**rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*fapolicyd*(8) *rpm-plugins*(8)
