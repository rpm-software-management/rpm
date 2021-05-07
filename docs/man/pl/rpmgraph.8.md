---
date: 30 czerwca 2002
section: 8
title: RPMGRAPH
---

NAZWA
=====

rpmgraph - Wyświetlanie grafu zależności pakietu RPM

SKŁADNIA
========

**rpmgraph** *PLIK\_PAKIETU \...*

OPIS
====

**rpmgraph** używa argumentów *PLIK\_PAKIETU* do wygenerowania grafu
zależności pakietów. Każdy argument *PLIK\_PAKIETU* jest czytany i
dodawany do zbioru transakcji rpm-a. Elementy zbioru transakcji są
częściowo porządkowane przy użyciu sortowania topologicznego. Następnie
częściowo uporządkowane elementy są wypisywane na standardowe wyjście.

Wierzchołki w grafie zależności to nazwy pakietów, krawędzie w grafie
skierowanym wskazują na rodzica każdego wierzchołka. Rodzic jest
zdefiniowany jako ostatni poprzednik pakietu w częściowym porządku przy
użyciu gdzie zależności pakietu jako relacji. Oznacza to, że rodzic
danego pakietu jest ostatnią zależnością pakietu.

Wyjście jest w formacie grafu skierowanego **dot**(1) i może być
wyświetlone lub wydrukowane przy użyciu edytora grafów **dotty** z
pakietu **graphviz**. Nie ma opcji specyficznych dla programu
**rpmgraph**, tylko wspólne opcje **rpm**-a. Aktualnie zaimplementowane
opcje można zobaczyć w komunikacie o składni komendy **rpmgraph**.

ZOBACZ TAKŻE
============

**dot**(1),

**dotty**(1),

** http://www.graphviz.org/ \<URL:http://www.graphviz.org/\>**

AUTORZY
=======

Jeff Johnson \<jbj\@redhat.com\>
