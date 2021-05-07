---
date: 22 December 1998
section: 8
title: rpm
---

MENO
====

rpm - Red Hat správca balíkov

POUŽITIE
========

**rpm** \[voľby\]

POPIS
=====

**rpm** je veľmi výkonný *správca balíkov*, ktorý môže byť použitý na
zostavenie, inštaláciu, výpis informácií, kontrolu, aktualizáciu a
odinštalovanie jednotlivých softverových balíkov. *Balík* obsahuje
archív súborov a informácií o balíku vrátane mena, verzie a popisu.

Musí byť použitý v niektorom z nasledujúcich režimov: *inicializácia
databázy*, *prebudovanie databázy*, *zostavenie balíka*, *rekompilácia
balíka*, *zostavenie balíka z tar archívu*, *zistenie informácií*,
*výpis informačných tagov*, *inštalácia*, *občerstvenie*,
*odinštalovanie*, *kontrola a overenie*, *overenie podpisu*, *opätovný
podpis*, *pridanie podpisu*, *nastavenie vlastníkov a skupín* a *výpis
konfigurácie*.

Spravovanie databázy:\
*** rpm -i \[\--initdb\]***\
*** rpm -i \[\--rebuilddb\]***

Zostavenie:\
*** rpm \[-b\|t\] \[balík\_spec\]+***\
*** rpm \[\--rebuild\] \[zdrojové\_rpm\]+***\
*** rpm \[\--tarbuild\] \[tarovaný\_zdroj\]+***\

Zistenie informácií:\
*** rpm \[\--query\] \[zisťovacie-voľby\]***\
*** rpm \[\--querytags\]***\

Spravovanie inštalovaných balíkov:\
*** rpm \[\--install\] \[inštalačné-voľby\] \[súbor\_balíka\]+***\
*** rpm \[\--freshen\|-F\] \[inštalačné-voľby\] \[súbor\_balíka\]+***\
*** rpm \[\--erase\|-e\] \[odinštalačné-voľby\] \[balík\]+***\
*** rpm \[\--verify\|-V\] \[overovacie-voľby\] \[balík\]+***\

Podpisy (signatúry):\
*** rpm \[\--verify\|-V\] \[overovacie-voľby\] \[balík\]+***\
*** rpm \[\--resign\] \[súbor\_balíka\]+***\
*** rpm \[\--addsign\] \[súbor\_balíka\]+***\

Rozličné:\
*** rpm \[\--showrc\]***\
*** rpm \[\--setperms\] \[balík\]+***\
*** rpm \[\--setgids\] \[balík\]+***\

VŠEOBECNÉ VOĽBY
===============

Tieto voľby môžu byť použité vo všetkých režimoch.

-   Vypíše množstvo ošklivých ladiacich informácií.

-   Vypíše čo najmenej informácií - normálne sa zobrazia iba chybové
    hlášky.

-   Vypíše o niečo dlhšiu informáciu o použití ako je bežný výpis.

-   Vypíše jednoriadkovú informáciu pozostávajúcu z čísla používanej
    verzie **rpm**.

-   Každý zo súborov v *\<zoznam\_súborov\>*** oddelených dvojbodkami
    je** postupne čítaný cez **rpm za účelom získania konfiguračných
    informácií.** Implicitný *\<zoznam súborov\>*** je**
    **/usr/lib/rpm/rpmrc:/etc/rpmrc:\~/.rpmrc.** Iba prvý súbor zo
    zoznamu súborov musí existovať, a vlnovky (\~) sú expandované na
    hodnotu **\$HOME.**

-   Použije adresár *\<adresár\>*** ako koreňový adresár systému pre
    všetky operácie.** Uvedomte si, že toto znamená, že databáza sa sa
    bude čítať alebo modifikovať pod adresárom *\<adresár\>*** a všetky
    predinštalačné a poinštalačné skripty** budú bežať po prevedení
    chroot() na adresár *\<adresár\>***.**

-   Použije databázu RPM v ceste *\<cesta\>***.**

-   Aktualizuje iba databázu a nie súborový systém.

-   Použije *\<počítač\>*** ako počítač s FTP alebo HTTP proxy
    serverom.** Bližšie podrobnosti sú uvedené v sekcii **VOĽBY
    FTP/HTTP.**

-   Použije *\<port\>*** ako FTP alebo HTTP port na počítači s proxy
    serverom.** Bližšie podrobnosti sú uvedené v sekcii **VOĽBY
    FTP/HTTP.**

-   Presmeruje výstup **rpm na príkaz ***\<príkaz\>***.**

VOĽBY PRI INŠTALÁCII A AKTUALIZÁCII (UPGRADE)
=============================================

Všeobecná forma inštalačného príkazu je

**rpm -i \[inštalačné-voľby\] ***\<súbor\_balíka\>+*

Táto voľba nainštaluje nový balíček. Všeobecná forma aktualizačného
príkazu je

**rpm -U \[inštalačné-voľby\] ***\<súbor\_balíka\>+*

Táto voľba aktualizuje existujúci nainštalovaný balíček alebo
nainštaluje nový balíček. Podobá sa inštalačnému príkazu, rozdiel je iba
v tom, že všetky ďaľšie verzie balíka sa odstránia zo systému.

**rpm \[-F\|\--freshen\] \[inštalačné-voľby\] ***\<súbor\_balíka\>+*

Táto voľba aktualizuje balíky, ale iba za predpokladu, že predchádzajúce
verzie sú nainštalované.

*\<súbor\_balíka\>*** môže byť špecifikovaný ako ftp alebo http URL,** v
tomto prípade sa najprv balíček stiahne a nainštaluje až potom. Bližšie
podrobnosti o vstavanej podpore ftp a http sú uvedené v sekcii **VOĽBY
FTP/HTTP.**

-   Rovnaký efekt ako súčasné použitie **\--replacepkgs, \--replacefiles
    a ** **\--oldpackage.**

-   Vypíše 50 znakov \#, keď je celý archív balíka rozbalený. Pre lepší
    vzhľad je vhodné používať s voľbou **-v.**

-   Povolí aktualizáciu prepísaním novšieho balíka starším.

-   Vypíše stav rozbalovania súborov z balíkového archívu v percentách.
    Toto je praktické pri behu RPM z iných nástrojov.

-   Nainštaluje balík aj v prípade, že nahradí súbory z iných, už
    nainštalovaných balíkov.

-   Nainštaluje balíky aj v prípade, že niektoré z nich už sú
    nainštalované v systéme.

-   Nainštaluje alebo aktualizuje všetky missingok súbory (súbory, ktoré
    nemusia nutne existovať) z balíka bez ohľadu na to, či existujú.

-   Nevykoná kontrolu závislostí pred inštalovaním alebo aktualizovaním
    balíka.

-   Nevykoná predinštalačné a poinštalačné skripty.

-   Nevykoná skripty, ktorých spúšťou je inštalácia balíka.

-   Nevykoná kontrolu pripojeného súborového systému na dostatok voľného
    miesta pred inštaláciou balíka.

-   Nenainštaluje súbory, ktorých mená začínajú na *\<cesta\>***.**

-   Nenainštaluje žiadne súbory, ktoré sú označené ako dokumentácia
    (ktoré zahŕňajú najmä manuálové stránky a texinfo dokumenty).

-   Nainštaluje súbory, ktoré sú označené ako dokumentácia. Toto je
    implicitné nastavenie.

-   Nenainštaluje balík, iba jednoducho otestuje, čo by sa vykonalo pri
    inštalácii a vypíše potenciálne konflikty.

-   Toto umožní inštaláciu alebo aktualizáciu aj v prípade, že sa
    nezhoduje architektúra popísaná v binárnom RPM a počítača, na ktorý
    sa má balík inštalovať.

-   Toto umožní inštaláciu alebo aktualizáciu aj v prípade, že sa
    nezhoduje operačný systém popísaný v binárnom RPM a počítači, na
    ktorý sa má balík inštalovať.

-   Toto nastaví inštalačný prefix na *\<cesta\>*** pre relokovateľné**
    (premiestniteľné) balíky.

-   Pre relokovateľné balíky, preloží cestu súborov, ktoré sa mali
    umiestniť na miesto *\<stará\_cesta\>*** do ***\<nová\_cesta\>***.**

-   Vnúti relokáciu aj v prípade, že balík nie je relokovateľný. Používa
    sa spolu s voľbou \--relocate.

-   Nezmení poradie balíkov na inštaláciu. V opačnomom prípade by mohlo
    byť zmenené poradie v zozname balíkov, aby sa zachovali závislosti.

VOĽBY PRI ZISTENÍ INFORMÁCIÍ (QUERY)
====================================

Všeobecná forma príkazu zistenia informácií je

**rpm -q \[zisťovacie-voľby\]**

Je možné špecifikovať, v akom formáte majú byť vypísané výstupné údaje.
Na takýto účel slúži voľba **\[\--queryformat\|\--qf\], nasledovaná**
formátovacím reťazcom.

Informačné výstupy sú modifikovanou verziou štandardného **printf(3)**
formátovania. Formát je vytvorený zo statických reťazcov (ktoré môžu
zahŕňať štandardné C znakové escape sekvencie pre nový riadok, tabelátor
a ďaľšie špeciálne znaky a **printf(3) typové formátovače). Keďže rpm
už** vie, aky typ má vytlačiť, špecifikátor typu musí byť vynechaný a
nahradený menom tagu hlavičky, ktorá má byť vytlačená, uzavretý znakmi
{}. RPMTAG\_ časť mena tagu môže byť vynechaná.

Alternatívny výstup formátovania môže byť požadovaný, ak je nasledovaný
tagom s **:***typetag*. Momentálne sú podporované nasledujúce typy:
**octal**, **date**, **shescape**, **perms**, **fflags**, a
**depflags**.

Napríklad na vytlačenie informácie o mene balíka je možné použiť
formátovací reťazec **%{NAME}**. Na vytlačenie informácie o mene a
distribúcii v dvoch stĺpcoch je možné použiť
**%-30{NAME}%{DISTRIBUTION}**.

**rpm** zobrazí zoznam všetkých tagov, ktoré pozná, keď je spustené s
argumentom **\--querytags**.

Existujú dve podmnožiny volieb pre zistenie informácií: výber balíka a
výber informácií.

Voľby výberu balíka:\

-   Zisťuje u inštalovaných balíkov s menom *\<meno\_balíka\>***.**

-   Zisťuje u všetkých nainštalovaných balíkov.

-   Zisťuje u všetkých inštalovaných balíkov, ktoré vyžadujú
    *\<schopnosť\>* pre správnu funkčnosť.

-   Zisťuje u všetkých inštalovaných balíkov, ktoré poskytujú vlastnosť
    *\<virtuálna\_schopnosť\>***.**

-   Zisťuje u balíka, ktorý vlastní súbor *\<file\>***.**

-   Zisťuje u balíkov, ktoré majú skupinu *\<group\>***.**

-   Zisťuje u (nenainštalovaného) balíka *\<súbor\_balíka\>***.**
    *\<súbor\_balíka\>*** môže byť špecifikovaný v ftp alebo http štýle
    URL,** v takomto prípade bude stiahnutá hlavička balíka, a z nej
    čítané požadované informácie. Bližšie informácie o vstavanej podpore
    ftp a http klienta sú v sekcii **FTP/HTTP VOĽBY.**

-   Rozanalyzuje *\<spec\_súbor\>*** a zisťuje informácie z tohto
    súboru, ako keby** to bol balík rpm. Aj napriek neprítomnosti
    všetkých informácií (napr. zoznam súborov), tento druh zisťovania
    umožňuje rpm získať informácie zo spec súboru bez nutnosti napísať
    špeciálny analyzátor spec súborov.

-   Zisťuje u záznamu číslo *\<číslo\>*** databázy priamo, toto je
    užitočné pre** ladiace účely.

-   Vypíše balíky, ktorých spúštou je existencia balíka *\<balík\>***.**

Voľba výberu informácií:\

-   Vypíše informácie o balíku, vrátane mena, verzie a popisu. Táto
    voľba využíva **\--queryformat, ak je špecifikovaný.**

-   Vypíše zoznam balíkov, na ktorých daný balík závisí.

-   Vypíše zoznam vlastností/schopností, ktoré poskytuje tento balík.

-   Vypíše históriu zmien pre balík.

-   Vypíše zoznam súborov v balíku.

-   Vypíše *stavy*** jednotlivých súborov v balíku (aplikuje voľbu**
    **-l). Stav jednotlivých súborov môže byť ***normal*** (normálny),**
    *not installed*** (nenainštalovaný) alebo ***replaced***
    (nahradený).**

-   Vypíše zoznam súborov označených ako dokumentácia (aplikuje **-l).**

-   Vypíše iba zoznam konfiguračných súborov (aplikuje **-l).**

-   Vypíše balíkovo špecifický shellový skript, ktorý je použitý v
    inštalačnom alebo odinštalačnom procese, ak nejaký vôbec existuje.

-   Vypíše skripty, ktoré sú spúšťané spúšťou, ak nejaká existuje a je
    obsiahnutá v balíku.

-   Vypíše zoznam podrobných vlastností súborov pozostoávajúci z: cesta
    veľkosť mtime md5sum mód vlastník skupina je\_konfiguračný\_súbor
    je\_dokumentačný\_súbor rdev symlink. Táto voľba musí byť použitá
    minimálne s jednou z nasledujúcich volieb **-l, -c, -d.**

-   Usporiada zoznam balíkov podľa času inštalácie takým spôsobom, že
    posledný inštalovaný balík bude na vrchu.

-   Vypíše zoznam všetkých súborov v každom balíku.

-   Vypíše všetky skripty, ktoré sú spúšťané spúšťou vo vybranom balíku.

VOĽBY PRI KONTROLE A OVEROVANÍ
==============================

Všeobecná forma príkazu kontroly je

**rpm -V\|-y\|\--verify \[overovacie-voľby\]**

Kontrola balíka prebieha z porovnania informácií z inštalovaných súborov
z balíkov v systéme s informáciami o súboroch, ktoré obsahoval pôvodný
balík (tieto su uložené v rpm databáze). Okrem iných údajov, kontrola
porovnáva veľkosť, MD5 kontrolný súčet, oprávnenia, typ, vlastníka a
skupinu každého súboru. Všetky odchýľky sú zobrazené. Specifikačné voľby
balíka sú rovnaké ako režime výpisu informácii balíkov.

Súbory, ktoré neboli inštalované z balíka, týkajúce sa napr.
dokumentačných súborov pri použití voľby \"**\--excludedocs\" pri
inštalácii, sú v** tichosti ignorované.

Voľby, ktoré môžu byť použité v kontrolnom režime:

-   Ignoruje chýbajúce súbory v systéme počas kontroly.

-   Ignoruje chyby kontrolných súčtov MD5 počas kontroly.

-   Ignoruje chyby PGP podpisov počas kontroly.

Výstup má formát 9 znakového reťazca, s prípadným rozšírením výskytu
\"**c\", ktoré charakterizuje konfiguračný súbor a mena súboru.** Každý
z ôsmych znakov popisuje výsledok porovnania jedného konkrétneho
atribútu súboru s údajmi zaznamenanými v RPM databáze. Jednoduchá
\"**.\"** (bodka) znamená, že test prešiel (neobjavené žiadne odchýľky).
Nasledujúce znaky oznamujú dôvod neúspechu určitého testu:

5.  MD5 kontrolný súčet

```{=html}
<!-- -->
```
S.  Veľkosť súboru

T.  Symbolický link

U.  Mtime (posledný čas modifikácie)

V.  Zariadenie

W.  Užívateľ

X.  Skupina

Y.  Mód (vrátane oprávnení a typu súborov)

KONTROLA PODPISOV (SIGNATURE)
=============================

Všeobecná forma príkazu kontroly rpm podpisu je

**rpm \--checksig ***\<súbor\_balíka\>+*

Takto sa overuje PGP podpis balíka *\<súbor\_balíka\>*** na uistenie**
sa o jeho integrite a pôvode. Konfiguračné informácie PGP sú čítané z
konfiguračných súborov. Bližšie údaje sa nachádzajú v sekcii PGP
POPDPISY.

VOĽBY PRI ODINŠTALÁCII
======================

Všeobecná forma príkazu na odinštalovanie je

** rpm -e ***\<meno\_balíka\>+*

-   Odstráni všetky verzie balíka, ktoré súhlasia s menom
    *\<meno\_balíka\>*. Normálne sa vyvolá chyba, ak viac balíkov
    súhlasí s menom *\<meno\_balíka\>*.

-   Nevykoná pred a po odinštalačné skripty.

-   Nevykoná skripty, ktorých spúšťou je odinštalovanie balíka.

-   Nebude skúmať závislosti pri odinštalovaní balíkov.

-   Nič sa v skutočnosti neodinštaluje, len sa preverí, čo by sa malo
    stať. Veľmi užitočné s voľbou **-vv**.

VOĽBY PRI ZOSTAVOVANÍ
=====================

Všeobecná forma príkazu na zostavenie rpm je

**rpm -\[b\|t\]***O*** \[zostavovacie-voľby\]
***\<spec\_súbor\_balíka\>+*

Argumentom je **-b**, ak sa na zostavenie balíka použije spec súbor
(súbor špecifikácií balíka) alebo **-t**, ak **RPM** má vyhľadať spec
súbor vnútri gzipovaného (alebo komprimovaného) tar archívu, a tento
použiť na zostavenie balíka. Po prvom argumente ďaľší argument (*O*)
špecifikuje fázu štádia zostavenia a zabalenia, ktorá sa má vykonať, a
ktorá je jedna z:

-   Vykoná \"%prep\" fázu pre spec súbor. Normálne toto vyvolá
    rozbalenie zdrojových archívov a aplikovanie záplat.

-   Vykoná \"kontrolu zoznamu\". Sekcia \"%files\" z spec súboru je
    makrom expandovaná, a je vykonaná kontrola, že každý súbor existuje.

-   Vykoná fázu \"%build\" pre spec súbor (po vykonaní prep fázy). Toto
    normálne vyvolá ekvivalent príkazu \"make\".

-   Vykoná fázu \"%install\" zo spec súboru (po vykonaní prep a build
    fázy). Toto vo všeobecnosti vyvolá ekvivalent príkazu \"make
    install\".

-   Zostaví binárny balík (po vykonaní prep, build a install fázy).

-   Zostaví iba zdrojový balík (po vykonaní prep, build a install fázy).

-   Zostaví binárny a zdrojový balík (po vykonaní prep, build a install
    fázy).

Môžu byť použité aj nasledujúce voľby:

-   Preskočí priamo na požadovanú fázu (t. zn. preskočí všetky fázy
    štádia zostavenia, ktoré predchádzajú špecifikovanej fáze). Táto
    voľba je platná iba s prepínačmi **-bc** and **-bi**.

-   Nastavuje \"kontrolu času\" (0 zakáže). Táto voľba môže byť
    nastavená definovaním makra \"\_timecheck\". Hodnota \"kontroly
    času\" vyjadruje (v sekundách) maximálny vek súborov, ktoré budú
    zabalené. Varovania sú vypisované pre všetky súbory, ktorých vek je
    za hranicou takto definovanej hodnoty.

-   Odstrání zostavovací strom (adresár) potom, čo sa vytvorí balík.

-   Odstrání zdrojové súbory a spec súbor po zostavení (môže sa používať
    aj samostatne, napr. \"**rpm \--rmsource foo.spec**\").

-   Nevykoná žiadnu zo zostavovacej fázy. Užitočné pre otestovanie spec
    súboru.

-   Vloží PGP podpis do balíka. Tento podpis môže byť využitý na
    overenie integrity a pôvodu balíka. Bližšie informácie na nastavenie
    sú uvedené v sekcii PGP PODPISY.

-   Pri zostavovaní balíka prepíše tag BuildRoot adresárom
    *\<adresár\>***.**

-   Pri zostavovaní balíka sa interpretuje *\<platforma\>*** ako**
    **arch-vendor-os a makrá \_target, \_target\_arch a** **\_target\_os
    sa nastavia podľa tejto hodnoty.**

VOĽBY PRI ZNOVUZOSTAVOVANÍ A REKOMPILOVANÍ
==========================================

Existujú dve voľby, ako spustiť zostavenie balíka s rpm:

***rpm \--recompile ***\<zdrojový\_súbor\_balíka\>+**

***rpm \--rebuild ***\<zdrojový\_súbor\_balíka\>+**

Keď je rpm spustené týmto spôsobom, **rpm nainštaluje zdrojový balík a**
vykoná postupne fázy prípravy (prep), kompilácie a inštalácie. Prídavne
s voľbou **\--rebuild zostaví nový binárny balík. Keď sa zostavenie**
dokončí, adresár zostavovania (ako pri **\--clean), zdrojové súbory,**
ako aj spec súbor sa sa odstránia.

PODPISOVANIE EXISTUJÚCEHO RPM
=============================

***rpm \--resign ***\<binárny\_súbor\_balíka\>+**

Táto voľba vygeneruje a vloží nový podpis pre zoznam balíkov. Všetky
prípadne existujúce podpisy sa odstránia.

***rpm \--addsign ***\<binárny\_súbor\_balíka\>+**

Táto voľba vygeneruje a pridá nový podpis pre zoznam balíkov u ktorých
už podpis existuje.

PGP PODPISY
===========

Aby bolo možné používať vlastnosti podpisovania, RPM je potrebné
nastaviť spôsobom, aby mohlo spúšťať PGP, a aby bolo schopné nájsť
zväzok verejných kľúčov s RPM verejnými kľúčmi v ňom. Implicitne RPM
používa implicitné hodnoty PGP na nájdenie zväzkov kľúčov (honorujúc
PGPPATH). Ak je zväzok kľúčov umiestnený na inom mieste, ako PGP
očakáva, je potrebné nastaviť makro

-   na definovanie umiestnenia zväzkov kľúčov PGP, ktoré sa majú použiť.

Ak si želáte podpisovať balíky, ktoré si sami vytvoríte, potrebujete
podobným spôsobom vytvoriť váš verejný a tajný kľúčový pár (bližšie
informácie sú v dokumentácii ku PGP). Taktiež potrebujete konfigurovať
makrá:

-   Meno \"užívateľa\", ktorého kľúčom sa má podpísať balík.

Pri zostavovaní balíka je potrebné pridať \--sign do príkazového riadku.
Nasledovne sa objaví výzva na heslo, a po správnom zadadaní sa balík
zostaví a podpíše.

Napríkad pre použitie PGP na podpísanie balíka ako užívateľ **\"John Doe
\<jdoe\@foo.com\>\" zo zväzku kľúčov umiestnených** v **/etc/rpm/.pgp
použitím /usr/bin/pgp zápis bude** obsahovať

***%\_pgp\_path /etc/rpm/.pgp***

***%\_pgp\_name John Doe \<jdoe\@foo.com\>***

***%\_pgpbin /usr/bin/pgp***

v konfiguračnom súbore makier: **/etc/rpm/macros je určený na**
per-systém nastavenie a **\~/.rpmmacros na per-užívateľ nastavenie.**

VOĽBY PRI PREBUDOVANÍ DATABÁZY
==============================

Všeobecná forma príkazu prebudovania databázy je

**rpm \--rebuilddb**

Na vybudovanie novej databázy treba vykonať

**rpm \--initdb**

Jedinými voľbami pre tento režim sú **\--dbpath a \--root.**

SHOWRC
======

Spustením

**rpm \--showrc**

sa vypíšu hodnoty, ktoré bude RPM používať pri všetkých voľbách, a ktoré
môžu byť nastavené v *rpmrc*** súboroch.**

FTP/HTTP VOĽBY
==============

RPM obsahuje jednoduchého FTP a HTTP klienta na zjednodušenie inštalácie
a jednoduchšieho získania informácií balíkov, ktoré sú umiestenené na
sieti. Súbory balíkov určené pre inštalovanie, aktualizáciu a výpis
informácií je možné špecifikovať v ftp alebo http štýle URL:

**ftp://\<užívateľ\>:\<heslo\>\@počítač:\<port\>/path/to/package.rpm**

Ak časť **:heslo chýba, objaví sa výzva na heslo (vždy len jeden krát**
pre pár užívateľ/počítač). Ak chýbajú obe časti - užívateľ aj heslo,
použitý je anonymný ftp. Vo všetkých prípadoch je použitý pasívny (PASV)
ftp prenos.

RPM povoluje použiť nasledujúce voľby s ftp URL:

-   Počítač *\<meno\_počítača\>*** sa použije ako proxy server pre
    všetky ftp** prenosy, čo umožní užívateľom použiť ftp služby za
    firewallom, ktorý používa proxy systémy. Táto voľba môže byť tak
    isto špecifikovaná nastavením makra **\_ftpproxy.**

```{=html}
<!-- -->
```
-   Použije sa číslo TCP portu *\<port\>*** pre ftp spojenie s ftp
    proxy** serverom namiesto implicitného portu. Táto voľba môže byť
    tak isto špecifikovaná nastavením makra **\_ftpport.**

RPM umožňuje nasledujúce voľby pri použití http URL:

-   Počítač *\<meno\_počítača\>*** bude použitý ako proxy server pre
    všetky http** prenosy. Táto voľba môže byť tak isto špecifikovaná
    konfigurovaním makra **\_httpproxy.**

```{=html}
<!-- -->
```
-   Použije sa číslo TCP portu *\<port\>*** pre http spojenie s http
    proxy** serverom namiesto implicitného portu. Táto voľba môže byť
    tiež špecifikovaná konfigurovaním makra **\_httpport.**

SÚBORY
======

    /usr/lib/rpm/rpmrc
    /etc/rpmrc
    ~/.rpmrc
    /usr/lib/rpm/macros
    /etc/rpm/macros
    ~/.rpmmacros
    /var/lib/rpm/conflictsindex.rpm
    /var/lib/rpm/fileindex.rpm
    /var/lib/rpm/groupindex.rpm
    /var/lib/rpm/nameindex.rpm
    /var/lib/rpm/packages.rpm
    /var/lib/rpm/providesindex.rpm
    /var/lib/rpm/requiredby.rpm
    /var/lib/rpm/triggerindex.rpm
    /tmp/rpm*

PRÍBUZNÁ DOKUMENTÁCIA
=====================

*glint*(8)*,* *rpm2cpio*(8)*,* **http://www.rpm.org/**

AUTORI
======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
