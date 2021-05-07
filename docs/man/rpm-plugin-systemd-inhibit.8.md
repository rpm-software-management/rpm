---
date: 14 Apr 2016
section: 8
title: 'RPM-SYSTEMD-INHIBIT'
---

NAME
====

rpm-plugin-systemd-inhibit - Plugin for the RPM Package Manager

Description
===========

This plugin for RPM prevents the system to enter shutdown, sleep or idle
mode while there is a rpm transaction running to prevent system
corruption that can occur if the transaction is interrupted by a reboot.

This is achieved by using the inhibit DBUS interface of systemd. The
call is roughly equivalent to executing

**systemd-inhibit \--mode=block \--what=idle:sleep:shutdown \--who=RPM
\--why=\"Transaction running\"**

See **systemd-inhibit**(1) for the details of this mechanism.

It is strongly advised to have the plugin installed on all systemd based
systems.

Prerequisites
=============

For the plugin to work systemd has to be used as init system and though
the DBUS system bus must be available. If the plugin cannot access the
interface it gives a warning but does not stop the transaction.

Configuration
=============

There are currently no options for this plugin in particular. See
**rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*systemd-inhibit*(1) *rpm*(8) *rpm-plugins*(8)
