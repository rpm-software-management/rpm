---
date: 10 stycznia 2000
section: 1
title: GENDIFF
---

NAZWA
=====

gendiff - narzędzie pomagające przy generowaniu bezbłędnych plików diff

SKŁADNIA
========

**gendiff** \<katalog\> \<rozszerzenie-diff\>

OPIS
====

**gendiff** jest dość prostym skryptem pomagającym przy generowaniu
pliku diff z pojedynczego katalogu. Jako jedyne argumenty przyjmuje
nazwę katalogu i \"rozszerzenie-diff\". Rozszerzenie diff powinno być
unikalną sekwencją znaków dodaną na końcu wszystkich oryginalnych, nie
zmodyfikowanych plików. Wyjściem programu jest plik diff, który można
nałożyć przy użyciu programu **patch**, aby odtworzyć zmiany.

Zwykle sekwencja czynności do stworzenia pliku diff to utworzenie dwóch
identycznych katalogów, dokonanie zmian w jednym katalogu i użycie
narzędzia **diff** do utworzenia listy różnic między nimi. Użycie
gendiff eliminuje potrzebę dodatkowej kopii oryginalnego, nie
zmodyfikowanego katalogu. Zamiast tego trzeba zachować tylko pojedyncze
pliki przed zmodyfikowaniem.

Przed edycją pliku skopiuj go, dołączając do nazwy wybrane rozszerzenie.
Tzn. jeśli zamierzasz zmodyfikować plik somefile.cpp i wybrałeś
rozszerzenie \"fix\", skopiuj go do somefile.cpp.fix przed edycją.
Następnie modyfikuj pierwszą kopię (somefile.cpp).

Po edycji wszystkich potrzebnych plików w ten sposób wejdź do katalogu
jeden poziom wyżej niż jest obecny kod źródłowy i napisz:

        $ gendiff tenkatalog .fix > mydiff-fix.patch

Powinieneś przekierować wyjście do pliku (jak na przykładzie), chyba że
chcesz zobaczyć wynik na standardowym wyjściu.

ZOBACZ TAKŻE
============

**diff**(1), **patch**(1)

AUTOR
=====

    Marc Ewing <marc@redhat.com>
