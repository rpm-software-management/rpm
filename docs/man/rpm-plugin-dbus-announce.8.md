---
date: 03 Jun 2020
section: 8
title: 'RPM-DBUS-ANNOUNCE'
---

NAME
====

rpm-plugin-dbus-announce - DBus plugin for the RPM Package Manager

Description
===========

The plugin writes basic information about rpm transactions to the system
dbus - like packages installed or removed. Other programs can subscribe
to the signals to be notified of the packages on the system change.

DBus Signals
============

Sends **StartTransaction** and **EndTransaction** messages from the
**/org/rpm/Transaction** object with the **org.rpm.Transaction**
interface.

The signal passes the DB cookie as a string and the transaction id as an
unsigned 32 bit integer.

Configuration
=============

There are currently no options for this plugin in particular. See
**rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*dbus-monitor*(1) *rpm-plugins*(8)
