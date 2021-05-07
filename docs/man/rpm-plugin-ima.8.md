---
date: 28 Jan 2020
section: 8
title: 'RPM-IMA'
---

NAME
====

rpm-plugin-ima - IMA plugin for the RPM Package Manager

Description
===========

Integrity Measurement Architecture (IMA) and the Linux Extended
Verification Module (EVM) allow to detect when files have been
accidentally or maliciously altered. This plugin puts IMA/EVM signatures
in the *security.ima* extended file attribute during installation. This
requires packages to contain the signatures - typically by being signed
with **rpmsign \--signfiles**.

Configuration
=============

The *%\_ima\_sign\_config\_files* macro controls whether signatures
should also be written for config files.

See **rpm-plugins**(8) on how to control plugins in general.

SEE ALSO
========

*evmctl*(1) *rpmsign*(8) *rpm*(8)
