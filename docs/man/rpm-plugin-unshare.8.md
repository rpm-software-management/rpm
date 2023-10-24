---
date: 15 Sep 2023
section: 8
title: 'RPM-UNSHARE'
---

NAME
====

rpm-plugin-unshare - Unshare plugin for the RPM Package Manager

Description
===========

This plugin allows using various Linux-specific namespace-related
technologies inside transactions, such as to harden and limit
scriptlet access to resources.

Configuration
=============

This plugin implements the following configurables:

`%__transaction_unshare_paths`

:   A colon-separated list of paths to privately mount during scriptlet
    execution. Typical examples would be `/tmp` to protect against
    insecure temporary file usage inside scriptlets, and `/home` to
    prevent scriptlets from accessing user home directories.

`%__transaction_unshare_nonet`

:   Non-zero value disables network access during scriptlet execution.

See **rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

**dbus-monitor**(1), **rpm-plugins**(8)
