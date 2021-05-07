---
date: 29 Jan 2020
section: 8
title: 'RPM-PLUGINS'
---

NAME
====

rpm-plugins - Plugins for the RPM Package Manager

Description
===========

RPM plugins provide functionality that is not suited to be used
everywhere. They may not be built or shipped on some platforms or may
not be installed or be disabled on some systems.

This allows plugins to interface with systems that may not acceptable as
a dependency for RPM and to provide functionality that may be unwanted
under some circumstances.

For now the plugin API is internal only. So there is a limited number of
plugins in the RPM sources.

Configuration
=============

Some plugins can be configured by specific macros or influenced by
command line parameters. But most can only be turned on or off. See the
plugin\'s man page for details.

Plugins are controlled by a macro *%\_\_transaction\_NAME* which is set
to the location of the plugin file. Undefining the macro or setting it
to *%{nil}* will prevent the plugin from being run.

This can be done on the RPM command line e.g. with
**\--undefine=\_\_transaction\_syslog**. To disable a plugin
permantently drop a file in */etc/rpm/* that contains

\_\_transaction\_NAME %{nil}

Another option is to remove the plugin from the system if it is packaged
in its own sub package.

For some operations it may be desirable to disable all plugins at once.
This can be done by passing **\--noplugins** to **rpm** at the command
line.

SEE ALSO
========

*rpm*(8) *rpm-plugin-audit*(8) *rpm-plugin-ima*(8)
*rpm-plugin-prioreset*(8) *rpm-plugin-selinux*(8) *rpm-plugin-syslog*(8)
*rpm-plugin-systemd-inhibit*(8)
