---
date: 9 czerwca 2002
section: 8
title: RPMBUILD
---

NAZWA
=====

rpmbuild - Budowanie pakietów RPM

SKŁADNIA
========

BUDOWANIE PAKIETÓW:
-------------------

**rpmbuild** {**-ba\|-bb\|-bp\|-bc\|-bi\|-bl\|-bs**}
\[**opcje-rpmbuild**\] *PLIK\_SPEC \...*

**rpmbuild** {**-ta\|-tb\|-tp\|-tc\|-ti\|-tl\|-ts**}
\[**opcje-rpmbuild**\] *TARBALL \...*

**rpmbuild** {**\--rebuild\|\--recompile**} *PAKIET\_ŹRÓDŁOWY \...*

RÓŻNE:
------

**rpmbuild** **\--showrc**

opcje-rpmbuild
--------------

\[**\--buildroot ***KATALOG*\] \[**\--clean**\] \[**\--nobuild**\]
\[**\--rmsource**\] \[**\--rmspec**\] \[**\--short-circuit**\]
\[**\--sign**\] \[**\--target ***PLATFORMA*\]

OPIS
====

**rpmbuild** służy do budowania binarnych i źródłowych pakietów
oprogramowania. **Pakiet** składa się z archiwum plików oraz metadanych
używanych do instalowania i usuwania plików. Metadane zawierają
pomocnicze skrypty, atrybuty plików oraz informacje opisujące pakiet.
**Pakiety** występują w dwóch wersjach: pakietach binarnych, służących
do opakowania oprogramowania do instalacji oraz pakietach źródłowych,
zawierających kod źródłowy i przepis na zbudowanie pakietów binarnych.

Trzeba wybrać jeden z następujących podstawowych trybów: **Budowanie
pakietu**, **Budowanie pakietu z tarballa**, **Rekompilacja pakietu**,
**Wyświetlenie konfiguracji**.

OPCJE OGÓLNE
------------

Opcje te mogą być używane we wszystkich trybach.

**-?**, **\--help**

:   Wypisuje informację o użyciu dłuższą niż zwykle.

**\--version**

:   Wypisuje pojedynczą linię, zawierającą numer wersji używanego
    **rpm**-a.

**\--quiet**

:   Wypisuje jak najmniej - zazwyczaj tylko komunikaty o błędach.

**-v**

:   Wypisuje szczegółowe informacje - zwykle komunikaty o przebiegu
    procesu.

**-vv**

:   Wypisuje dużo brzydkich informacji diagnostycznych.

**\--rcfile ***LISTA\_PLIKÓW*

:   Każdy z plików w oddzielonej dwukropkami *LIŚCIE\_PLIKÓW* jest
    odczytywany kolejno przez **rpm**-a w poszukiwaniu informacji o
    konfiguracji. Istnieć musi tylko pierwszy plik z listy, a tyldy są
    zamieniane na wartość **\$HOME**. Domyślną *LISTĄ\_PLIKÓW* jest
    */usr/lib/rpm/rpmrc*:*/usr/lib/rpm/\<vendor\>/rpmrc*:*/etc/rpmrc*:*\~/.rpmrc*.

**\--pipe ***KOMENDA*

:   Przekazuje potokiem wyjście **rpm**-a do *KOMENDY*.

**\--dbpath ***KATALOG*

:   Używa bazy danych z *KATALOGU* zamiast domyślnego */var/lib/rpm*.

**\--root ***KATALOG*

:   Używa do wszystkich operacji systemu zakorzenionego w *KATALOGU*.
    Zauważ, że oznacza to, że baza danych w *KATALOGU* będzie używana
    przy sprawdzaniu zależności, a wszystkie skrypty (np. **%post** przy
    instalacji pakietu lub **%prep** przy budowaniu pakietu) będą
    uruchamiane po chroot(2) na *KATALOG*.

OPCJE BUDOWANIA
---------------

Ogólną postacią komendy budowania rpm-a jest

**rpmbuild** **-b***ETAP***\|-t***ETAP* \[ ** opcje-rpmbuild** \] *PLIK
\...*

Jeśli do zbudowania pakietu używany jest plik spec, to argumentem
powinno być **-b**, a jeśli **rpmbuild** powinien zajrzeć wewnątrz (być
może skompresowanego) pliku tar w poszukiwaniu speca, to powinna być
użyta opcja **-t**. Po pierwszym argumencie, drugi znak (*ETAP*) określa
etapy budowania i pakietowania, które należy wykonać. Może być jednym z:

**-ba**

:   Buduje pakiety binarny i źródłowy (po wykonaniu etapów %prep, %build
    i %install).

**-bb**

:   Buduje pakiet binarny (po wykonaniu etapów %prep, %build i
    %install).

**-bp**

:   Wykonuje etap \"%prep\" z pliku spec. Zwykle obejmuje to
    rozpakowanie źródeł i zaaplikowanie wszelkich łat.

**-bc**

:   Wykonuje etap \"%build\" z pliku spec (po wykonaniu etapu %prep).
    Ogólnie obejmuje to odpowiednik \"make\".

**-bi**

:   Wykonuje etap \"%install\" z pliku spec (po wykonaniu etapów %prep i
    %build). Ogólnie obejmuje to odpowiednik \"make install\".

**-bl**

:   Dokonuje sprawdzenia listy. W sekcji \"%files\" pliku spec rozwijane
    są makra i dokonywane są sprawdzenia, by upewnić się, że każdy plik
    istnieje.

**-bs**

:   Buduje tylko pakiet źródłowy.

Mogą być też użyte następujące opcje:

**\--buildroot ***KATALOG*

:   Na czas budowania pakietu zastępuje wartość BuildRoot *KATALOGIEM*.

**\--clean**

:   Usuwa drzewo budowania po stworzeniu pakietów.

**\--nobuild**

:   Nie wykonuje żadnych etapów budowania. Przydatne do testowania
    plików spec.

**\--rmsource**

:   Usuwa źródła po budowaniu (może być też używane samodzielnie, np.
    \"**rpmbuild** **\--rmsource foo.spec**\").

**\--rmspec**

:   Usuwa plik spec po budowaniu (może być też używane samodzielnie, np.
    \"**rpmbuild** **\--rmspec foo.spec**\").

**\--short-circuit**

:   Przechodzi wprost do podanego etapu (tzn. pomija wszystkie etapy
    prowadzące do podanego). Prawidłowe tylko z **-bc** i **-bi**.

**\--sign**

:   Osadza w pakiecie sygnaturę GPG. Sygnatura ta może być używana do
    weryfikowania integralności i pochodzenia pakietu. Zobacz sekcję o
    SYGNATURACH GPG w **rpm**(8), gdzie znajdują się szczegóły dotyczące
    konfiguracji.

**\--target ***PLATFORMA*

:   Podczas budowania pakietu interpretuje *PLATFORMĘ* jako
    **arch-vendor-os** i ustawia odpowiednio makra **%\_target**,
    **%\_target\_cpu** oraz **%\_target\_os**.

OPCJE PRZEBUDOWYWANIA I REKOMPILACJI
------------------------------------

Istnieją dwa inne sposoby na wywołanie budowania przy użyciu rpm-a:

**rpmbuild** **\--rebuild\|\--recompile** *PAKIET\_ŹRÓDŁOWY \...*

Po takim wywołaniu, **rpmbuild** instaluje podany pakiet źródłowy oraz
wykonuje etapy prep, kompilacji i instalacji. Dodatkowo, **\--rebuild**
buduje nowy pakiet binarny. Po tym jak budowanie jest zakończone,
katalog budowania jest usuwany (jak przy **\--clean**), a potem źródła i
plik spec dla pakietu są usuwane.

WYŚWIETLANIE KONFIGURACJI
-------------------------

Polecenie

**rpmbuild** **\--showrc**

pokazuje wartości, których **rpmbuild** będzie używał dla wszystkich
opcji, które są aktualnie ustawione w plikach konfiguracyjnych *rpmrc*
oraz *macros*.

PLIKI
=====

Konfiguracja rpmrc
------------------

    /usr/lib/rpm/rpmrc
    /usr/lib/rpm/<vendor>/rpmrc
    /etc/rpmrc
    ~/.rpmrc

Konfiguracja makr
-----------------

    /usr/lib/rpm/macros
    /usr/lib/rpm/<vendor>/macros
    /etc/rpm/macros
    ~/.rpmmacros

Baza danych
-----------

    /var/lib/rpm/Basenames
    /var/lib/rpm/Conflictname
    /var/lib/rpm/Dirnames
    /var/lib/rpm/Filemd5s
    /var/lib/rpm/Group
    /var/lib/rpm/Installtid
    /var/lib/rpm/Name
    /var/lib/rpm/Packages
    /var/lib/rpm/Providename
    /var/lib/rpm/Provideversion
    /var/lib/rpm/Pubkeys
    /var/lib/rpm/Removed
    /var/lib/rpm/Requirename
    /var/lib/rpm/Requireversion
    /var/lib/rpm/Sha1header
    /var/lib/rpm/Sigmd5
    /var/lib/rpm/Triggername

Tymczasowe
----------

*/var/tmp/rpm\**

ZOBACZ TAKŻE
============

    popt(3),
    rpm2cpio(8),
    gendiff(1),
    rpm(8),

**http://www.rpm.org/ \<URL:http://www.rpm.org/\>**

AUTORZY
=======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
