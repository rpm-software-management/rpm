---
date: 24 października 2002
section: 8
title: RPMDEPS
---

NAZWA
=====

rpmdeps - Generowanie zależności pakietów RPM

SKŁADNIA
========

**rpmdeps** **{-P\|\--provides}** **{-R\|\--requires}** *PLIK \...*

OPIS
====

**rpmdeps** generuje zależności pakietu dla zbioru argumentów
**PLIKOWYCH**. Każdy argument **PLIKOWY** jest przeszukiwany pod kątem
Elf32/Elf64, interpretera skryptów lub zależności dla skryptu, a
zależności są wypisywane na standardowe wyjście.

ZOBACZ TAKŻE
============

**rpm**(8),

**rpmbuild**(8),

AUTORZY
=======

Jeff Johnson \<jbj\@redhat.com\>
