---
date: 9 czerwca 2002
section: 8
title: rpm
---

NAZWA
=====

rpm - Menedżer pakietów RPM

SKŁADNIA
========

ODPYTYWANIE I WERYFIKACJA PAKIETÓW:
-----------------------------------

**rpm** {**-q\|\--query**} \[**opcje-wyboru**\] \[**opcje-zapytań**\]

**rpm** {**-V\|\--verify**} \[**opcje-wyboru**\]
\[**opcje-weryfikacji**\]

**rpm** **\--import** *KLUCZ\_PUBLICZNY \...*

**rpm** {**-K\|\--checksig**} \[**\--nosignature**\] \[**\--nodigest**\]
*PLIK\_PAKIETU \...*

INSTALOWANIE, UAKTUALNIANIE I USUWANIE PAKIETÓW:
------------------------------------------------

**rpm** {**-i\|\--install**} \[**opcje-instalacji**\] *PLIK\_PAKIETU
\...*

**rpm** {**-U\|\--upgrade**} \[**opcje-instalacji**\] *PLIK\_PAKIETU
\...*

**rpm** {**-F\|\--freshen**} \[**opcje-instalacji**\] *PLIK\_PAKIETU
\...*

**rpm** {**-e\|\--erase**} \[**\--allmatches**\] \[**\--nodeps**\]
\[**\--noscripts**\] \[**\--notriggers**\] \[**\--test**\]
*NAZWA\_PAKIETU \...*

RÓŻNE:
------

**rpm** {**\--initdb\|\--rebuilddb**}

**rpm** {**\--addsign\|\--resign**} *PLIK\_PAKIETU \...*

**rpm** {**\--querytags\|\--showrc**}

**rpm** {**\--setperms\|\--setugids**} *NAZWA\_PAKIETU \...*

opcje-wyboru
------------

\[*NAZWA\_PAKIETU*\] \[**-a,\--all**\] \[**-f,\--file ***PLIK*\]
\[**-g,\--group ***GRUPA*\] {**-p,\--package ***PLIK\_PAKIETU*\]
\[**\--hdrid ***SHA1*\] \[**\--pkgid ***MD5*\] \[**\--tid ***TID*\]
\[**\--querybynumber ***NUMER\_NAGŁÓWKA*\] \[**\--triggeredby
***NAZWA\_PAKIETU*\] \[**\--whatprovides ***WŁASNOŚĆ*\]
\[**\--whatrequires ***WŁASNOŚĆ*\]

opcje-zapytań
-------------

\[**\--changelog**\] \[**-c,\--configfiles**\] \[**-d,\--docfiles**\]
\[**\--dump**\] \[**\--filesbypkg**\] \[**-i,\--info**\] \[**\--last**\]
\[**-l,\--list**\] \[**\--provides**\] \[**\--qf,\--queryformat
***FORMAT\_ZAPYTANIA*\] \[**-R,\--requires**\] \[**\--scripts**\]
\[**-s,\--state**\] \[**\--triggers,\--triggerscripts**\]

opcje-weryfikacji
-----------------

\[**\--nodeps**\] \[**\--nofiles**\] \[**\--noscripts**\]
\[**\--nodigest**\] \[**\--nosignature**\] \[**\--nolinkto**\]
\[**\--nomd5**\] \[**\--nosize**\] \[**\--nouser**\] \[**\--nogroup**\]
\[**\--nomtime**\] \[**\--nomode**\] \[**\--nordev**\]

opcje-instalacji
----------------

\[**\--aid**\] \[**\--allfiles**\] \[**\--badreloc**\]
\[**\--excludepath ***STARA\_ŚCIEŻKA*\] \[**\--excludedocs**\]
\[**\--force**\] \[**-h,\--hash**\] \[**\--ignoresize**\]
\[**\--ignorearch**\] \[**\--ignoreos**\] \[**\--includedocs**\]
\[**\--justdb**\] \[**\--nodeps**\] \[**\--nodigest**\]
\[**\--nosignature**\] \[**\--nosuggest**\] \[**\--noorder**\]
\[**\--noscripts**\] \[**\--notriggers**\] \[**\--oldpackage**\]
\[**\--percent**\] \[**\--prefix ***NOWA\_ŚCIEŻKA*\] \[**\--relocate
***STARA\_ŚCIEŻKA***=***NOWA\_ŚCIEŻKA*\] \[**\--replacefiles**\]
\[**\--replacepkgs**\] \[**\--test**\]

OPIS
====

**rpm** jest potężnym **menedżerem pakietów**, który może być używany do
budowania, instalowania, odpytywania, weryfikowania, uaktualniania i
usuwania pakietów oprogramowania. **Pakiet** składa się z archiwum
plików oraz metadanych używanych do instalowania i usuwania plików.
Metadane zawierają pomocnicze skrypty, atrybuty plików oraz informacje
opisujące pakiet. **Pakiety** występują w dwóch wersjach: pakietach
binarnych, służących do opakowania oprogramowania do instalacji oraz
pakietach źródłowych, zawierających kod źródłowy i przepis na zbudowanie
pakietów binarnych.

Należy wybrać jeden z następujących podstawowych trybów:
**Odpytywania**, **Weryfikowania**, **Sprawdzania sygnatury**,
**Instalowania/Uaktualniania/Odświeżania**, **Odinstalowywania**,
**Inicjalizowania bazy danych**, **Przebudowywania bazy danych**,
**Ponownego podpisywania**, **Dodawania sygnatury**, **ustawiania
właścicieli i grup**, **Pokazywania etykiet zapytań**, oraz
**Pokazywania konfiguracji**.

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

OPCJE INSTALOWANIA I UAKTUALNIANIA
----------------------------------

Ogólną postacią komendy instalowania rpm-a jest

**rpm** {**-i\|\--install**} \[**install-options**\] *PLIK\_PAKIETU
\...*

Instaluje to nowy pakiet.

Ogólną postacią komendy uaktualniania rpm-a jest

**rpm** {**-U\|\--upgrade**} \[**install-options**\] *PLIK\_PAKIETU
\...*

Uaktualnia to aktualnie zainstalowany lub instaluje pakiet w nowej
wersji. Jest to to samo co install, lecz wszystkie inne wersje pakietu
będą usunięte po zainstalowaniu nowego pakietu.

**rpm** {**-F\|\--freshen**} \[**install-options**\] *PLIK\_PAKIETU
\...*

Odświeży to pakiety, lecz tylko jeśli wcześniejsza wersja już istnieje.
*PLIK\_PAKIETU* może być podany jako URL **ftp** lub **http**. W tym
wypadku pakiet zostanie pobrany przed zainstalowaniem. W sekcji **OPCJE
FTP/HTTP** znajduje się więcej informacji o wewnętrznej obsłudze
klienckiej **ftp** i **http** w **rpm**.

**\--aid**

:   Dodaje w razie potrzeby sugerowane pliki do zbioru transakcji.

**\--allfiles**

:   Instaluje lub odświeża wszystkie pliki missingok (takie, których
    może brakować) z pakietu, niezależnie czy istnieją.

**\--badreloc**

:   Do użytku w połączeniu z **\--relocate**. Pozwala na relokowanie
    ścieżek wszystkich plików, nie tylko tych, których *STARA\_ŚCIEŻKA*
    jest na liście podpowiedzi dla relokacji w pakiecie binarnym.

**\--excludepath ***STARA\_ŚCIEŻKA*

:   Nie instaluje plików, których nazwy rozpoczynają się od
    *STARA\_ŚCIEŻKA*.

**\--excludedocs**

:   Nie instaluje żadnych plików, które są zaznaczone jako dokumentacja
    (co tyczy się także podręczników man i texinfo).

**\--force**

:   To samo, co użycie: **\--replacepkgs**, **\--replacefiles** i
    **\--oldpackage**.

**-h**, **\--hash**

:   Wypisuje 50 znaków krzyżyka, pokazując proces rozpakowywania
    archiwum. Używając z **-v\|\--verbose**, uzyskasz ładny obraz.

**\--ignoresize**

:   Nie sprawdza, czy na zamontowanych systemach plików jest dość
    miejsca na zainstalowanie tego pakietu.

**\--ignorearch**

:   Umożliwia instalację lub uaktualnienie nawet w wypadku, gdy
    architektury binarnego pakietu i hosta nie odpowiadają sobie.

**\--ignoreos**

:   Umożliwia instalację lub uaktualnienie nawet w wypadku, gdy systemy
    operacyjne binarnego pakietu i hosta nie odpowiadają sobie.

**\--includedocs**

:   Instaluje pliki dokumentacji. Tak jest domyślnie.

**\--justdb**

:   Odświeża tylko bazę danych, a nie system plików.

**\--nodigest**

:   Nie weryfikuje skrótów kryptograficznych pakietu ani nagłówka przy
    odczycie.

**\--nosignature**

:   Nie weryfikuje sygnatur pakietu ani nagłówka przy odczycie.

**\--nodeps**

:   Nie dokonuje sprawdzenia zależności przed instalowaniem, lub
    uaktualnieniem pakietu.

**\--nosuggest**

:   Nie sugeruje pakietu(ów), które dostarczają brakującą zależność.

**\--noorder**

:   Nie porządkuje pakietów do instalacji. Lista pakietów w normalnych
    wypadkach jest porządkowana na nowo, aby spełnić zależności.

**\--noscripts**

:   

**\--nopre**

:   

**\--nopost**

:   

**\--nopreun**

:   

**\--nopostun**

:   Nie wywołuje skryptów o podanej nazwie. Opcja **\--noscripts** jest
    równoważna

**\--nopre** **\--nopost** **\--nopreun** **\--nopostun**

i wyłącza wykonywanie odpowiadających im skryptów **%pre**, **%post**,
**%preun** oraz **%postun**.

**\--notriggers**

:   

**\--notriggerin**

:   

**\--notriggerun**

:   

**\--notriggerpostun**

:   Nie wywołuje skryptów, które są pociągane przez instalację lub
    usuwanie pakietu. Opcja **\--notriggers** jest równoważna

**\--notriggerin** **\--notriggerun** **\--notriggerpostun**

i wyłącza wykonywanie odpowiadających im skryptów **%triggerin**,
**%triggerun** oraz **%triggerpostun**.

**\--oldpackage**

:   Zezwala uaktualnianiu na zastąpienie nowszego pakietu starszym.

**\--percent**

:   Wypisuje procenty podczas rozpakowywania plików z archiwum. Jest to
    zrobione w celu ułatwienia wywoływania pm-a z innych narzędzi.

**\--prefix ***NOWA\_ŚCIEŻKA*

:   Dla pakietów relokowalnych tłumaczy wszystkie ścieżki plików
    zaczynające się od prefiksu instalacji w podpowiedziach dla
    relokacji na OWĄ\_ŚCIEŻKĘ.

**\--relocate ***STARA\_ŚCIEŻKA***=***NOWA\_ŚCIEŻKA*

:   Dla pakietów relokowalnych tłumaczy wszystkie ścieżki plików
    zaczynające się od *STAREJ\_ŚCIEŻKI* w podpowiedziach dla relokacji
    na *NOWĄ\_ŚCIEŻKĘ*. Ta opcja może używana wiele razy, jeśli ma być
    zrelokowane kilka różnych *STARYCH\_ŚCIEŻEK*.

**\--replacefiles**

:   Instaluje pakiety nawet jeśli zastępują one pliki z innych, już
    zainstalowanych pakietów.

**\--replacepkgs**

:   Instaluje pakiety nawet jeśli niektóre z nich są już zainstalowane
    na tym systemie.

Nie instaluje pakietu, po prostu sprawdza i raportuje potencjalne

:   konflikty.

OPCJE USUWANIA
--------------

Ogólną postacią komendy usuwania rpm-a jest

**rpm** {**-e\|\--erase**} \[**\--allmatches**\] \[**\--nodeps**\]
\[**\--noscripts**\] \[**\--notriggers**\] \[**\--test**\]
*NAZWA\_PAKIETU \...*

Można użyć następujących opcji:

**\--allmatches**

:   Usunie wszystkie wersje pakietu, które odpowiadają
    *\<NAZWIE\_PAKIETU*. Normalnie wyświetlany jest błąd, gdy nazwa ta
    odpowiada wielu pakietom.

**\--nodeps**

:   Nie sprawdza zależności przed odinstalowaniem.

**\--noscripts**

:   

**\--nopreun**

:   

**\--nopostun**

:   Nie wywołuje skryptów o podanej nazwie. Opcja **\--noscripts** przy
    usuwaniu pakietów jest równoważna

**\--nopreun** **\--nopostun**

i wyłącza wykonywanie odpowiadających im skryptów **%preun** oraz
**%postun**.

**\--notriggers**

:   

**\--notriggerun**

:   

**\--notriggerpostun**

:   Nie wywołuje skryptów, które są pociągane przez usunięcie pakietu.
    Opcja **\--notriggers** jest równoważna

**\--notriggerun** **\--notriggerpostun**

i wyłącza wykonywanie odpowiadających im skryptów **%triggerun** oraz
**%triggerpostun**.

**\--test**

:   Nie odinstalowuje niczego naprawdę, przechodzi tylko przez kolejne
    etapy. Przydatne w połączeniu z opcją **-vv** w celach
    diagnostycznych.

OPCJE ZAPYTAŃ
-------------

Ogólną postacią komendy zapytania rpm-a jest

**rpm** {**-q\|\--query**} \[**opcje-wyboru**\] \[**opcje-zapytań**\]

Można podać format, w jakim powinna zostać wypisywana informacja o
pakiecie. Aby tego dokonać, użyj opcji

**\--qf\|\--queryformat** *FORMAT\_ZAPYTANIA*

z dołączonym łańcuchem formatującym *FORMAT\_ZAPYTANIA*. Formaty zapytań
są zmodyfikowanymi wersjami standardowego formatowania **printf(3)**.
Format jest złożony ze statycznych łańcuchów (które mogą zawierać
standardowe znaki specjalne C - dla nowych linii, tabulacji itp.) oraz
formatek typu, podobnych do tych z **printf(3)**. Ponieważ **rpm** już
zna typ do wypisania, specyfikacja typu jest pomijana. W jej miejsce
wchodzi nazwa etykiety wypisywanego nagłówka, ujęta w znaki **{}**.
Nazwy etykiet nie są wrażliwe na wielkość liter, a początkowa część
**RPMTAG\_** nazwy etykiety może być opuszczona.

Można zażądać innych formatów wyjściowych przez zakończenie etykiety
**:***znacznik\_typu*. Obecnie obsługiwane są następujące typy:

**:armor**

:   Pakuje klucz publiczny w osłonę ASCII.

**:base64**

:   Koduje dane binarne przy w base64.

**:date**

:   Używa formatu \"%c\" strftime(3).

**:day**

:   Używa formatu \"%a %b %d %Y\" strftime(3).

**:depflags**

:   Formatuje flagi zależności.

**:fflags**

:   Formatuje flagi plików.

**:hex**

:   Formatuje szesnastkowo.

**:octal**

:   Formatuje ósemkowo.

**:perms**

:   Formatuje uprawnienia plików.

**:shescape**

:   Zabezpiecza pojedyncze cudzysłowy do użycia w skrypcie.

**:triggertype**

:   Wyświetla przyrostek skryptów pociąganych.

Na przykład aby wypisać tylko nazwy odpytywanych pakietów, można użyć
jako łańcucha formatującego samego **%{NAME}**. Aby wypisać nazwy
pakietów i informacje o dystrybucji w dwóch kolumnach, można użyć
**%-30{NAME}%{DISTRIBUTION}** (Nazwa będzie w 30 znakowym okienku, z
wyrównaniem do lewej - zobacz printf(3) - przyp. tłum.)

**rpm** uruchomiony z argumentem **\--querytags** wypisze listę
wszystkich znanych etykiet.

Istnieją dwa podzbiory opcji dla odpytywania: wybór pakietu i wybór
informacji.

OPCJE WYBORU PAKIETU:
---------------------

*NAZWA\_PAKIETU*

:   Odpytuje zainstalowany pakiet o nazwie *NAZWA\_PAKIETU*.

**-a**, **\--all**

:   Odpytuje wszystkie zainstalowane pakiety.

**-f**, **\--file ***PLIK*

:   Odpytuje pakiet będący właścicielem *PLIKU*.

**-g**, **\--group ***GRUPA*

:   Odpytuje pakiety o grupie *GRUPA*.

**-p**, **\--package ***PLIK\_PAKIETU*

:   Odpytuje (nie zainstalowany) pakiet *PLIK\_PAKIETU*. Plik ten może
    być podany jako URL w stylu **ftp** lub **http**. W takiej sytuacji,
    przed odpytaniem plik zostanie pobrany. W sekcji **OPCJE FTP/HTTP**
    znajduje się więcej informacji o wewnętrznej obsłudze klienckiej
    **ftp** i **http** w **rpm**-ie. Argumenty *PLIK\_PAKIETU* nie
    będące pakietami binarnymi są interpretowane jako pliki manifest w
    formacie ASCII. Dopuszczalne są komentarze zaczynające się od
    \'\#\', a każda linia pliku manifest może zawierać oddzielone
    odstępami wyrażenia glob, włącznie z URL-ami ze zdalnymi wyrażeniami
    glob, które będą rozwijane na ścieżki podstawiane w miejsce pliku
    manifest jako dodatkowe *PLIKI\_PAKIETU* do odpytania.

**\--pkgid***MD5*

:   Odpytuje pakiet zawierający podany identyfikator pakietu, będący
    skrótem *MD5* połączonego nagłówka i zawartości danych.

**\--querybynumber ***NUMBER\_NAGŁÓWKA*

:   Odpytuje bezpośrednio wpis z bazy o tym *NUMERZE\_NAGŁÓWKA*;
    przydatne tylko do diagnostyki.

**\--specfile ***PLIK\_SPEC*

:   Przetwarza i odpytuje *PLIK\_SPEC* tak, jakby był pakietem. Chociaż
    nie jest dostępna cała informacja (np. lista plików), to ten typ
    zapytań umożliwia używanie rpm-a do wyciągania informacji z plików
    spec bez potrzeby pisania specyficznego parsera.

**\--tid ***TID*

:   Odpytuje pakiet(y) o podanym identyfikatorze transakcji *TID*.
    Aktualnie jako identyfikator używany jest uniksowy znacznik czasu
    (timestamp). Wszystkie pakiety instalowane lub usuwane w pojedynczej
    transakcji mają wspólny identyfikator.

**\--triggeredby ***NAZWA\_PAKIETU*

:   Odpytuje pakiety, które są pociągnięte przez pakiety
    *NAZWA\_PAKIETU*.

**\--whatprovides ***WŁASNOŚĆ*

:   Odpytuje wszystkie pakiety udostępniające podaną *WŁASNOŚĆ*.

**\--whatrequires ***WŁASNOŚĆ*

:   Odpytuje wszystkie pakiety wymagające do poprawnego działania
    podanej *WŁASNOŚCI*.

OPCJE ZAPYTANIA PAKIETU:
------------------------

**\--changelog**

:   Wyświetla informacje o zmianach dla tego pakietu.

**-c**, **\--configfiles**

:   Listuje tylko pliki konfiguracyjne (wymusza **-l**).

**-d**, **\--docfiles**

:   Listuje tylko pliki dokumentacji (wymusza **-l**).

**\--dump**

:   Wyrzuca informacje o pliku w następujący sposób:

>     ścieżka rozmiar czas_mod suma_md5 prawa właściciel grupa konfig dokum rdev symlink

Ta opcja musi być użyta z przynajmniej jednym z **-l**, **-c**, **-d**.
**\--filesbypkg** Listuje wszystkie pliki z każdego z pakietów.

**-i**, **\--info**

:   Wyświetla informację o pakiecie zawierające nazwę, wersję i opis. O
    ile podano **\--queryformat**, to jest on używany.

**\--last**

:   Porządkuje listing pakietów podczas instalowania tak, że ostatnie
    pakiety są na górze.

**-l**, **\--list**

:   Listuje pliki z pakietu.

**\--provides**

:   Listuje właściwości, które udostępnia pakiet.

**-R**, **\--requires**

:   Listuje pakiety, od których zależy ten pakiet.

**\--scripts**

:   Listuje specyficzne dla pakietu skrypty, które są używane jako część
    procesu instalowania i odinstalowywania.

**-s**, **\--state**

:   Wyświetla *stany* plików w pakiecie (wymusza **-l**). Stan każdego
    pliku może być jednym z *normalny*, *niezainstalowany* lub
    *zastąpiony*.

**\--triggers**, **\--triggerscripts**

:   Wyświetla skrypty wywoływane przez inne pakiety (triggery) zawarte w
    pakiecie.

OPCJE WERYFIKACJI
-----------------

Ogólną postacią komendy weryfikacji rpm-a jest

**rpm** {**-V\|\--verify**} \[**opcje-wyboru**\]
\[**opcje-weryfikacji**\]

Weryfikowanie pakietu porównuje informacje o zainstalowanych plikach w
pakiecie z informacją o plikach pobraną z oryginalnego pakietu,
zapisanego w bazie rpm-a. Wśród innych rzeczy, porównywane są rozmiary,
sumy MD5, prawa, typ, właściciel i grupa każdego pliku. Wszystkie
niezgodności są natychmiast wyświetlane. Pliki, które nie były
zainstalowane z pakietu, jak na przykład dokumentacja przy instalacji z
opcją \"**\--excludedocs**\", są po cichu ignorowane.

Opcje wyboru pakietów są takie same jak dla odpytywania pakietów
(włącznie z plikami manifest jako argumentami). Inne opcje unikalne dla
trybu weryfikacji to:

**\--nodeps**

:   Nie weryfikuje zależności pakietów.

**\--nodigest**

:   Nie weryfikuje skrótów kryptograficznych nagłówka ani pakietu.

**\--nofiles**

:   Nie weryfikuje żadnych atrybutów plików pakietu przy odczycie.

**\--noscripts**

:   Nie wykonuje skryptów **%verifyscript** (nawet jeśli są).

**\--nosignature**

:   Nie weryfikuje sygnatur pakietu ani nagłówka przy odczycie.

**\--nolinkto**

:   

**\--nomd5**

:   

**\--nosize**

:   

**\--nouser**

:   

**\--nogroup**

:   

**\--nomtime**

:   

**\--nomode**

:   

**\--nordev**

:   Nie weryfikuje odpowiednich atrybutów plików.

Format wyjścia to łańcuch 9 znaków, z możliwym znacznikiem atrybutu:

    c %config plik konfiguracyjny.
    d %doc plik dokumentacji.
    g %ghost plik nie istniejący (nie dołączony do danych pakietu).
    l %license plik licencji.
    r %readme plik przeczytaj-to.

z nagłówka pakietu, zakończonych nazwą pliku. Każdy z 9 znaków oznacza
wynik porównania jednego atrybutu pliku z wartością atrybutu zapisaną w
bazie danych. Pojedyncza \"**.**\" (kropka) oznacza, że test przeszedł
pomyślnie, natomiast pojedynczy \"**?**\" (znak zapytania) oznacza, że
test nie mógł być przeprowadzony (na przykład uprawnienia pliku
uniemożliwiają odczyt). W pozostałych przypadkach znak oznacza
niepowodzenie odpowiadającego mu testu **\--verify**:

    S (Size) - rozmiar pliku się różni
    M (Mode) - tryb (uprawnienia lub typ) pliku się różni
    5 (MD5) - suma MD5 się różni
    D (Device) - numery główny/poboczny urządzenia się nie zgadzają
    L (Link) - ścieżka dowiązania się nie zgadza
    U (User) - właściciel pliku się różni
    G (Grupa) - grupa pliku się różni
    T (mTime) - czas modyfikacji pliku się różni

WERYFIKACJA CYFROWEJ SYGNATURY I SKRÓTU
---------------------------------------

Ogólne postacie komend związanych z sygnaturami cyfrowymi to

**rpm** **\--import** *KLUCZ\_PUBLICZNY \...*

**rpm** {**\--checksig**} \[**\--nosignature**\] \[**\--nodigest**\]
*PLIK\_PAKIETU \...*

Opcja **\--checksig** sprawdza wszystkie skróty kryptograficzne i
sygnatury zawarte w *PLIKU\_PAKIETU*, aby zapewnić jego integralność i
pochodzenie. Zauważ, że sygnatury są teraz weryfikowane przy każdym
odczycie pakietu, a **\--checksig** jest przydatne do zweryfikowania
wszystkich skrótów i sygnatur związanych z pakietem.

Sygnatury cyfrowe nie mogą być zweryfikowane bez klucza publicznego.
Klucz publiczny w opakowaniu ASCII może być dodany do bazy **rpm**-a
przy użyciu **\--import**. Zaimportowany klucz publiczny jest
przechowywany w nagłówku, a zarządzanie pierścieniem kluczy wykonuje się
dokładnie tak samo, jak zarządzanie pakietami. Na przykład, wszystkie
aktualnie zaimportowane klucze publiczne można wyświetlić przez:

**rpm -qa gpg-pubkey\***

Szczegółowe informacje o konkretnym kluczu publicznym po zaimportowaniu
mogą być wyświetlone przez odpytywanie. Oto informacje o kluczu GPG/DSA
Red Hata:

**rpm -qi gpg-pubkey-db42a60e**

Na koniec, klucze publiczne mogą być usunięte po zaimportowaniu tak samo
jak pakiety. Oto jak usunąć klucz GPG/DSA Red Hata:

**rpm -e gpg-pubkey-db42a60e**

PODPISYWANIE PAKIETU
--------------------

**rpm** **\--addsign\|\--resign** *PLIK\_PAKIETU \...*

Obie opcje, **\--addsign** i **\--resign** generują i umieszczają nowe
sygnatury dla każdego podanego pakietu *PLIK\_PAKIETU*, zastępując
wszystkie istniejące sygnatury. Dwie opcje istnieją z przyczyn
historycznych, aktualnie nie ma różnic w ich zachowaniu.

UŻYWANIE GPG TO PODPISYWANIA PAKIETÓW
-------------------------------------

Aby podpisać pakiety przy użyciu GPG, **rpm** musi być skonfigurowany,
aby mógł uruchamiać GPG i odnaleźć pierścień kluczy z odpowiednimi
kluczami. Domyślnie **rpm** używa przy szukaniu kluczy tych samych
konwencji co GPG, czyli zmiennej środowiskowej **\$GNUPGHOME**. Jeśli
pierścienie kluczy nie są zlokalizowane tam, gdzie GPG ich oczekuje,
trzeba skonfigurować makro **%\_gpg\_path** aby wskazywała na
lokalizację pierścieni kluczy GPG, które mają być używane.

Dla kompatybilności ze starszymi wersjami GPG, PGP oraz rpm-a, powinny
być skonfigurowane tylko pakiety sygnatur OpenPGP V3. Mogą być używane
algorytmy weryfikacji DSA lub RSA, ale DSA jest preferowany.

Jeśli chcesz podpisywać pakiety, które sam tworzysz, musisz też utworzyć
swój własny klucz publiczny i poufny (zobacz podręcznik GPG). Będziesz
też potrzebował skonfigurować makra **rpm**-a:

**%\_gpg\_name**

:   Nazwa \"użytkownika\", którego klucz będzie używany do podpisu.

Na przykład, aby użyć GPG do podpisania pakietów jako użytkownik *\"John
Doe \<jdoe\@foo.com\>\"* z pierścieni kluczy zlokalizowanych w
**/etc/rpm/.pgp**, przy użyciu programu */usr/bin/gpg*, załączyłbyś

    %_gpg_path /etc/rpm/.gpg
    %_gpg_name John Doe <jdoe@foo.com>
    %_gpgbin /usr/bin/gpg

w pliku konfiguracji makr. Do ogólnosystemowej konfiguracji użyj
*/etc/rpm/macros*, a dla lokalnej *\~/.rpmmacros*.

OPCJE PRZEBUDOWYWANIA BAZY DANYCH
---------------------------------

Ogólna postać komendy przebudowywania bazy danych rpm-a to

**rpm** {**\--initdb\|\--rebuilddb**} \[**-v**\] \[**\--dbpath
***KATALOG*\] \[**\--root ***KATALOG*\]

Użyj **\--initdb** aby utworzyć nową bazę danych lub **\--rebuilddb**,
aby przebudować indeksy bazy danych z nagłówków zainstalowanych
pakietów.

WYŚWIETLANIE KONFIGURACJI
-------------------------

Polecenie

**rpm** **\--showrc**

pokazuje wartości, których **rpm** będzie używał dla wszystkich opcji,
które są aktualnie ustawione w plikach konfiguracyjnych *rpmrc* oraz
*macros*.

OPCJE FTP/HTTP
--------------

**rpm** może działać jako klient FTP i/lub HTTP, co pozwala na
odpytywanie lub instalowanie pakietów z Internetu. Pliki pakietów do
operacji instalacji, uaktualnienia lub odpytania mogą być podane jako
URL w stylu **ftp** lub **http**:

ftp://UŻYTKOWNIK:HASŁO\@HOST:PORT/ścieżka/do/pakietu.rpm

Jeśli część **:HASŁO** jest pominięta, użytkownik zostanie o nie
zapytany (jednokrotnie na parę użytkownik/host). Jeśli pominięto nazwę
użytkownika i hasło, używany jest anonimowy **ftp**. We wszystkich
przypadkach używane są pasywne (PASV) transfery **ftp**.

**rpm** zezwala na używanie z URL-ami **ftp** następujących opcji:

**\--ftpproxy ***HOST*

:   Podany *HOST* będzie używany jako proxy dla wszystkich transferów
    ftp, co umożliwia użytkownikom ściąganie danych przez zapory
    ogniowe, które używają systemów proxy. Opcja ta może być też podana
    przez skonfigurowanie makra **%\_ftpproxy**.

**\--ftpport ***PORT*

:   Numer *PORTU* TCP, którego użyć do połączenia ftp na serwerze proxy
    zamiast portu domyślnego. Opcja ta może być też podana przez
    skonfigurowanie makra **%\_ftpport**.

**rpm** zezwala na używanie z URL-ami **http** następujących opcji:

**\--httpproxy ***HOST*

:   Podany *HOST* będzie używany jako proxy dla wszystkich transferów
    **http**. Opcja ta może być też podana przez skonfigurowanie makra
    **%\_httpproxy**.

**\--httpport ***PORT*

:   Numer *PORTU* TCP, którego użyć do połączenia **http** na serwerze
    proxy zamiast portu domyślnego. Opcja ta może być też podana przez
    skonfigurowanie makra **%\_httpport**.

SPRAWY SPADKOWE
===============

Uruchamianie rpmbuild
---------------------

Tryby budowania rpm-a znajdują się teraz w programie
*/usr/bin/rpmbuild*. Mimo że spadkowa kompatybilność zapewniona przez
wymienione niżej aliasy popt jest wystarczająca, kompatybilność nie jest
doskonała; dlatego kompatybilność trybu budowania poprzez aliasy popt
jest usuwana z rpm-a. Zainstaluj pakiet **rpm-build** i zobacz
**rpmbuild**(8), gdzie znajduje się dokumentacja wszystkich trybów
budowania **rpm** poprzednio udokumentowana w niniejszym **rpm**(8).

Dodaj następujące linie do */etc/popt*, jeśli chcesz nadal uruchamiać
**rpmbuild** z linii poleceń **rpm**-a:

    rpm     exec --bp               rpmb -bp
    rpm     exec --bc               rpmb -bc
    rpm     exec --bi               rpmb -bi
    rpm     exec --bl               rpmb -bl
    rpm     exec --ba               rpmb -ba
    rpm     exec --bb               rpmb -bb
    rpm     exec --bs               rpmb -bs 
    rpm     exec --tp               rpmb -tp 
    rpm     exec --tc               rpmb -tc 
    rpm     exec --ti               rpmb -ti 
    rpm     exec --tl               rpmb -tl 
    rpm     exec --ta               rpmb -ta
    rpm     exec --tb               rpmb -tb
    rpm     exec --ts               rpmb -ts 
    rpm     exec --rebuild          rpmb --rebuild
    rpm     exec --recompile        rpmb --recompile
    rpm     exec --clean            rpmb --clean
    rpm     exec --rmsource         rpmb --rmsource
    rpm     exec --rmspec           rpmb --rmspec
    rpm     exec --target           rpmb --target
    rpm     exec --short-circuit    rpmb --short-circuit

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
    rpmbuild(8),

**http://www.rpm.org/ \<URL:http://www.rpm.org/\>**

AUTORZY
=======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
