---
date: 11 stycznia 2001
section: 8
title: rpm2cpio
---

NAZWA
=====

rpm2cpio - konwersja pakietu RPM na archiwum cpio

SKŁADNIA
========

**rpm2cpio** \[ nazwa\_pliku \]

OPIS
====

**rpm2cpio** konwertuje podany, jako jedyny argument, plik .rpm do
postaci archiwum cpio na standardowym wyjściu. Jeśli podano argument
\'-\', to strumień rpm czytany jest ze standardowego wejścia.

\
***rpm2cpio rpm-1.1-1.i386.rpm***\
***rpm2cpio \< glint-1.0-1.i386.rpm***\
***rpm2cpio glint-1.0-1.i386.rpm \| cpio -dium***

ZOBACZ TAKŻE
============

*rpm*(8)

AUTOR
=====

    Erik Troan <ewt@redhat.com>
