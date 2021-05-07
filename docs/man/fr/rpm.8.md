---
date: 06 juin 2001
section: 8
title: RPM
---

NOM
===

rpm - Gestionnaire de Paquetages Red Hat

SYNOPSIS
========

INTERROGER ET VÉRIFIER DES PAQUETAGES :
---------------------------------------

**rpm** {**-q\|\--query**} \[**options-sélection**\]
\[**options-interrogation**\]

**rpm** {**-V\|\--verify**} \[**options-sélection**\] \[**\--nodeps**\]
\[**\--nofiles**\] \[**\--nomd5**\] \[**\--noscripts**\]

**rpm** {**-K\|\--checksig**} \[**\--nogpg**\] \[**\--nopgp**\]
\[**\--nomd5**\] *FICHIER\_PAQUETAGE \...*

INSTALLER, METTRE À NIVEAU ET DÉSINSTALLER DES PAQUETAGES :
-----------------------------------------------------------

**rpm** {**-i\|\--install**} \[**options-installation**\]
*FICHIER\_PAQUETAGE \...*

**rpm** {**-U\|\--upgrade**} \[**options-installation**\]
*FICHIER\_PAQUETAGE \...*

**rpm** {**-F\|\--freshen**} \[**options-installation**\]
*FICHIER\_PAQUETAGE \...*

**rpm** {**-e\|\--erase**} \[**\--allmatches**\] \[**\--nodeps**\]
\[**\--noscripts**\] \[**\--notriggers**\] \[**\--test**\]
*NOM\_PAQUETAGE \...*

CONSTRUIRE DES PAQUETAGES :
---------------------------

**rpm** {**-ba\|-bb\|-bp\|-bc\|-bi\|-bl\|-bs**}
\[**options-construction**\] *FICHIER\_SPECS \...*

**rpm** {**-ta\|-tb\|-tp\|-tc\|-ti\|-tl\|-ts**}
\[**options-construction**\] *TARBALL \...*

**rpm** {**\--rebuild\|\--recompile**} *PAQUETAGE\_SOURCE \...*

DIVERS :
--------

**rpm** {**\--initdb\|\--rebuilddb**}

**rpm** {**\--addsign\|\--resign**} *FICHIER\_PAQUETAGE \...*

**rpm** {**\--querytags\|\--showrc**}

**rpm** {**\--setperms\|\--setugids**} *NOM\_PAQUETAGE \...*

OPTIONS DE SÉLECTION
--------------------

**\[***NOM\_PAQUETAGE***\] \[-a,\--all\] \[-f,\--file ***FICHIER***\]**
\[-g,\--group *GROUPE***\] \[-p,\--package ***FICHIER\_PAQUETAGE***\]**
\[\--querybynumber *NOMBRE***\] \[\--triggeredby
***NOM\_PAQUETAGE***\]** \[\--whatprovides *CAPACITÉ***\]
\[\--whatrequires ***CAPACITÉ***\]**

OPTIONS D\'INTERROGATION
------------------------

**\[\--changelog\] \[-c,\--configfiles\] \[-d,\--docfiles\] \[\--dump\]
\[\--filesbypkg\]** \[-i,\--info\] \[\--last\] \[-l,\--list\]
\[\--provides\] \[\--qf,\--queryformat *FORMAT\_REQUÊTE***\]
\[-R,\--requires\] \[\--scripts\]** \[-s,\--state\]
\[\--triggers,\--triggerscripts\]

OPTIONS D\'INSTALLATION
-----------------------

**\[\--allfiles\] \[\--badreloc\] \[\--excludepath
***ANCIEN\_CHEMIN***\]** \[\--excludedocs\] \[\--force\] \[-h,\--hash\]
\[\--ignoresize\] \[\--ignorearch\] \[\--ignoreos\] \[\--includedocs\]
\[\--justdb\] \[\--nodeps\] \[\--noorder\] \[\--noscripts\]
\[\--notriggers\] \[\--oldpackage\] \[\--percent\] \[\--prefix
*NOUVEAU\_CHEMIN***\] \[\--relocate
***ANCIEN\_CHEMIN***=***NOUVEAU\_CHEMIN***\]** \[\--replacefiles\]
\[\--replacepkgs\] \[\--test\]

OPTIONS DE CONSTRUCTION
-----------------------

**\[\--buildroot ***RÉPERTOIRE***\] \[\--clean\] \[\--nobuild\]
\[\--rmsource\] \[\--rmspec\]** \[\--short-circuit\] \[\--sign\]
\[\--target *PLATE-FORME***\]**

DESCRIPTION
===========

**rpm** est un puissant *gestionnaire de paquetages*, qui peut être
utilisé pour construire, installer, interroger, vérifier, mettre à jour,
et désinstaller des paquetages de logiciels individuels. Un *paquetage*
est constitué d\'une archive de fichiers, et de méta-données utilisées
pour installer et supprimer les fichiers de l\'archive. Les méta-données
incluent les scripts assistants, les attributs des fichiers, et des
informations décrivant le paquetage. Il y a deux types de **paquetages**
: les paquetages binaires, utilisés pour encapsuler des logiciels à
installer, et les paquetages de sources, qui contiennent le code et la
recette permettant de produire des paquetages binaires.

Un des modes de base suivants doit être sélectionné : **Interroger**,
**Vérifier**, **Vérifier Signature**, **Installer/Mettre à
niveau/Rafraîchir**, **Désinstaller**, **Construire Paquetage**,
**Construire Paquetage à partir d\'un Tarball** (NdT : sources au format
.tar.xx), **Recompiler Paquetage**, **Initialiser Base de Données**,
**Reconstruire Base de Données**, **Resigner**, **Ajouter Signature**,
**Fixer Propriétaires/Groupes**, **Montrer les Options
d\'Interrogation**, et **Montrer Configuration**.

OPTIONS GÉNÉRALES
-----------------

Ces options peuvent être utilisées dans tous les modes.

**-?, \--help**

:   Afficher un message d\'utilisation plus long que de coutume.

**\--version**

:   Afficher une ligne unique contenant le numéro de version du **rpm**
    utilisé.

**\--quiet**

:   Afficher le moins possible - normalement, seuls les messages
    d\'erreur seront affichés.

**-v**

:   Afficher des informations verbeuses - les messages de progression
    des routines seront normalement affichés

**-vv**

:   Afficher un tas d\'horribles informations de débogage.

**\--rcfile ***LISTE\_FICHIERS*

:   Chacun des fichiers de *LISTE\_FICHIERS* (qui sont séparés par des
    deux-points) est lu séquentiellement par **rpm** pour obtenir des
    informations de configuration. Seul le premier fichier de la liste
    doit exister, et les tildes seront développés en **\$HOME**. La
    *LISTE\_FICHIERS* par défaut est
    */usr/lib/rpm/rpmrc*:*/etc/rpmrc*:*\~/.rpmrc*.

**\--pipe ***COMMANDE*

:   Envoyer la sortie de **rpm** à la *COMMANDE* par l\'intermédiaire
    d\'un tube.

**\--dbpath ***RÉPERTOIRE*

:   Utiliser la base de données située dans *RÉPERTOIRE* au lieu du
    chemin par défaut */var/lib/rpm*.

**\--root ***RÉPERTOIRE*

:   Utiliser le système ayant *RÉPERTOIRE* comme racine pour toutes les
    opérations. Notez que cela signifie que la base de données sera lue
    ou modifiée dans *RÉPERTOIRE* et que chacun des scriptlets (petits
    scripts) **%pre** et/ou **%post** sera exécuté après un chroot(2)
    vers *RÉPERTOIRE*.

OPTIONS D\'INSTALLATION ET DE MISE À NIVEAU
-------------------------------------------

La forme générale d\'une commande d\'installation rpm est

**rpm** {**-i\|\--install**} \[**options-installation**\]
*FICHIER\_PAQUETAGE \...*

Cela installe un nouveau paquetage.

La forme générale d\'une commande de mise à niveau rpm est

**rpm** {**-U\|\--upgrade**} \[**options-installation**\]
*FICHIER\_PAQUETAGE \...*

Cela met à niveau ou installe le paquetage actuellement installé vers
une version plus récente. C\'est similaire à l\'installation, sauf que
toutes les anciennes versions du paquetage sont désinstallées après que
le nouveau paquetage ait été installé.

**rpm** {**-F\|\--freshen**} \[**options-installation**\]
*FICHIER\_PAQUETAGE \...*

Cela mettra à niveau les paquetages, mais seulement si une version plus
ancienne existe à cet instant. Le *FICHIER\_PAQUETAGE* peut être
spécifié en tant qu\'URL **ftp** ou **http**, auquel cas le paquetage
sera téléchargé avant d\'être installé. Voyez **OPTIONS FTP/HTTP** pour
des informations sur le support interne d\'un client **ftp** et **http**
par **rpm**.

**\--allfiles**

:   Installer ou mettre à niveau tous les fichiers manquants du
    paquetage, même s\'ils existent déjà.

**\--badreloc**

:   Utilisé avec **\--relocate**, permet des relogements dans tous les
    chemins de fichiers, et pas seulement dans les *ANCIEN\_CHEMIN*
    inclus dans les indications de relogement du paquetage binaire.

**\--excludepath ***ANCIEN\_CHEMIN*

:   Ne pas installer de fichier dont le nom commence par
    *ANCIEN\_CHEMIN*.

**\--excludedocs**

:   Ne pas installer de fichier marqué comme faisant partie de la
    documentation (ce qui inclut les pages de manuel et les documents
    texinfo).

**\--force**

:   Similaire à l\'utilisation de **\--replacepkgs**,
    **\--replacefiles**, et **\--oldpackage**.

**-h, \--hash**

:   Afficher 50 marques de hachage quand l\'archive du paquetage est
    déballée. À utiliser avec **-v\|\--verbose** pour un plus bel
    affichage.

**\--ignoresize**

:   Ne pas vérifier s\'il y a un espace disque suffisant sur les
    systèmes de fichiers montés avant d\'installer ce paquetage.

**\--ignorearch**

:   Permettre l\'installation ou la mise à niveau même si les
    architectures du paquetage binaire et de l\'hôte ne correspondent
    pas.

**\--ignoreos**

:   Permettre l\'installation ou la mise à niveau même si les systèmes
    d\'exploitation du paquetage binaire et de l\'hôte ne concordent
    pas.

**\--includedocs**

:   Installer les fichiers de documentation. C\'est le comportement par
    défaut.

**\--justdb**

:   Ne mettre à jour que la base de données, et pas le système de
    fichiers.

**\--nodeps**

:   Ne pas effectuer de vérification des dépendances avant d\'installer
    ou de mettre à niveau un paquetage.

**\--noorder**

:   Ne pas réordonner les paquetages lors d\'une installation. La liste
    des paquetages devrait normalement être réordonnée pour satisfaire
    aux dépendances.

**\--noscripts**

:   

**\--nopre**

:   

**\--nopost**

:   

**\--nopreun**

:   

**\--nopostun**

:   Ne pas exécuter le scriptlet de même nom. L\'option **\--noscripts**
    est équivalente à

**\--nopre** **\--nopost** **\--nopreun** **\--nopostun**

et désactive l\'exécution des scriptlets correspondants **%pre**,
**%post**, **%preun**, et **%postun**.

**\--notriggers**

:   

**\--notriggerin**

:   

**\--notriggerun**

:   

**\--notriggerpostun**

:   Ne pas exécuter de scriptlet déclenché du type spécifié. L\'option
    **\--notriggers** est équivalente à

**\--notriggerin** **\--notriggerun** **\--notriggerpostun**

et désactive l\'exécution des scriptlets correspondants **%triggerin**,
**%triggerun**, et **%triggerpostun**.

**\--oldpackage**

:   Permettre qu\'une mise à niveau remplace un paquetage par un
    paquetage plus ancien.

**\--percent**

:   Afficher le pourcentage de progression de l\'extraction des fichiers
    de l\'archive du paquetage, afin de faciliter l\'exécution de
    **rpm** depuis d\'autres outils.

**\--prefix ***NOUVEAU\_CHEMIN*

:   Pour les paquetages binaires relogeables, traduire tous les chemins
    de fichiers présents dans les indications de relogement du
    paquetage, et débutant par le préfixe d\'installation, par
    *NOUVEAU\_CHEMIN*.

**\--relocate ***ANCIEN\_CHEMIN***=***NOUVEAU\_CHEMIN*

:   Pour les paquetages binaires relogeables, traduire tous les chemins
    de fichiers présents dans les indications de relogement du paquetage
    et débutant par *ANCIEN\_CHEMIN* par *NOUVEAU\_CHEMIN*. Cette option
    peut être utilisée de façon répétitive si plusieurs *ANCIEN\_CHEMIN*
    du paquetage doivent être relogés.

**\--replacefiles**

:   Installer les paquetages même s\'ils remplacent des fichiers
    d\'autres paquetages déjà installés.

**\--replacepkgs**

:   Installer les paquetages même si certains d\'entre eux sont déjà
    installés sur ce système.

**\--nobuild**

:   Ne pas installer le paquetage, mais uniquement rechercher et
    rapporter des conflits potentiels.

OPTIONS D\'INTERROGATION
------------------------

La forme générale d\'une commande d\'interrogation rpm est

**rpm** {**-q\|\--query**} \[**options-sélection**\]
\[**options-interrogation**\]

Vous pouvez spécifier le format dans lequel les informations sur le
paquetage doivent être affichées. Pour ce faire, utilisez l\'option
{**\--qf\|\--queryformat**}, suivie par la chaîne de format
*FORMAT\_REQUÊTE*. Les chaînes de format sont des versions modifiées de
celles du **printf(3)** standard. Le format est constitué de chaînes de
caractères statiques (qui peuvent inclure les séquences d\'échappement
de caractère C standard pour les sauts de lignes, tabulations et autres
caractères spéciaux) et de formateurs de type **printf(3)**. Comme
**rpm** connaît déjà le type à afficher, le spécificateur de type doit
néanmoins être omis, et être remplacé par le nom de l\'étiquette
d\'en-tête à afficher, enfermé dans des caractères **{}**. Les noms
d\'étiquettes sont insensibles à la casse, et la partie **RPMTAG\_** du
nom de l\'étiquette peut également être omise.

Des formats de sortie alternatifs peuvent être requis en faisant suivre
l\'étiquette par **:***typetag*. Actuellement, les types suivants sont
supportés : **octal**, **date**, **shescape**, **perms**, **fflags**,
and **depflags**. Par exemple, pour n\'afficher que le nom des
paquetages interrogés, vous pourriez utiliser **%{NAME}** comme chaîne
de format. Pour afficher les noms de paquetages et les informations de
distribution en deux colonnes, vous pourriez utiliser
**%-30{NAME}%{DISTRIBUTION}**. **rpm** affichera une liste de tous les
étiquettes qu\'il connaît quand il est invoqué avec l\'argument
**\--querytags**.

Il y a deux sous-ensembles d\'options d\'interrogation : la sélection de
paquetage, et la sélection d\'informations.

OPTIONS DE SÉLECTION DE PAQUETAGES :
------------------------------------

*NOM\_PAQUETAGE*

:   Interroger le paquetage installé nommé *NOM\_PAQUETAGE*.

**-a, \--all**

:   Interroger tous les paquetages installés.

**-f, \--file ***FICHIER*

:   Interroger le paquetage possédant le *FICHIER*.

**-g, \--group ***GROUPE*

:   Interroger le paquetage de groupe *GROUPE*.

**-p, \--package ***FICHIER\_PAQUETAGE*

:   Interroger un paquetage (non installé) *FICHIER\_PAQUETAGE*. Le
    *FICHIER\_PAQUETAGE* peut être spécifié en tant qu\'URL de style
    **ftp** ou **http**, auquel cas l\'en-tête du paquetage sera
    téléchargé et interrogé. Voyez **OPTIONS FTP/HTTP** pour obtenir des
    informations sur le support interne d\'un client ftp et http par
    RPM. Le ou les arguments *FICHIER\_PAQUETAGE*, s\'ils ne sont pas
    des paquetages binaires, seront interprétés comme étant un manifeste
    ascii de paquetage. Les commentaires sont autorisés ; ils débutent
    par un « \# », et chaque ligne d\'un fichier de manifeste de
    paquetage peut inclure des motifs génériques (y compris ceux
    spécifiant des URLs distantes) séparés par des espaces, qui seront
    développés en chemins qui remplacent le manifeste du paquetage par
    les arguments *FICHIER\_PAQUETAGE* additionnels ajoutés à la
    requête.

**\--querybynumber ***NOMBRE*

:   Interroger directement la *NOMBRE*-ième entrée de la base de données
    ; n\'est utile que pour le débogage.

**\--specfile ***FICHIER\_SPECS*

:   Analyse syntaxiquement et interroge le *FICHIER\_SPECS* (NdT :
    fichier de spécifications) comme s\'il s\'agissait d\'un paquetage.
    Bien que toutes les informations (p.ex. les listes de fichiers) ne
    soient pas disponibles, ce type d\'interrogation permet à rpm
    d\'être utilisé pour extraire des informations de fichiers specs
    sans devoir écrire un analyseur syntaxique de fichiers de
    spécifications.

**\--triggeredby ***NOM\_PAQUETAGE*

:   Interroger les paquetages qui sont déclenchés par le(s) paquetage(s)
    *NOM\_PAQUETAGE*.

**\--whatprovides ***CAPACITÉ*

:   Interroger tous les paquetages qui fournissent la capacité
    *CAPACITÉ*.

**\--whatrequires ***CAPACITÉ*

:   Interroger tous les paquetages qui requièrent *CAPACITÉ* pour un
    fonctionnement correct.

OPTIONS D\'INTERROGATION DE PAQUETAGE :
---------------------------------------

**\--changelog**

:   Afficher les informations concernant les changements dans ce
    paquetage.

**-c, \--configfiles**

:   Lister uniquement les fichiers de configuration (implique **-l**).

**-d, \--docfiles**

:   Lister uniquement les fichiers de documentation (implique **-l**).

**\--dump**

:   Afficher les informations sur le fichier comme suit :

>      chemin taille date_modif somme_md5 mode propriétaire
>      groupe isconfig isdoc rdev symlink

Cette option doit être utilisée avec au moins une option parmi **-l**,
**-c**, **-d**.

**\--filesbypkg**

:   Lister tous les fichiers de chaque paquetage sélectionné.

**-i, \--info**

:   Afficher des informations sur le paquetage, incluant son nom, sa
    version et sa description. Utilise l\'option **\--queryformat** si
    elle a été spécifiée.

**\--last**

:   Ordonner le listing des paquetages par date d\'installation de sorte
    que les derniers paquetages installés apparaissent en premier lieu.

**-l, \--list**

:   Lister les fichiers du paquetage.

**\--provides**

:   Lister les capacités que fournit ce paquetage.

**-R, \--requires**

:   Lister les paquetages desquels dépend ce paquetage.

**\--scripts**

:   Lister les scriplets spécifiques au paquetage qui sont utilisés
    comme partie intégrante des processus d\'installation et de
    désinstallation.

**-s, \--state**

:   Afficher les *états* des fichiers du paquetage (implique **-l**).
    L\'état de chaque fichier est *normal*, *non installé* ou
    *remplacé*.

**\--triggers, \--triggerscripts**

:   Afficher les scripts déclenchés qui sont contenus dans le paquetage
    (s\'il y en a).

OPTIONS DE VÉRIFICATION
-----------------------

La forme générale d\'une commande de vérification rpm est

**rpm** {**-V\|\--verify**} \[**options-sélection**\] \[**\--nodeps**\]
\[**\--nofiles**\] \[**\--nomd5**\] \[**\--noscripts**\]

La vérification d\'un paquetage compare les informations sur les
fichiers installés dans le paquetage avec les informations sur les
fichiers obtenues à partir des méta-données du paquetage original
conservées dans la base de données rpm. Entre autres choses, la
vérification compare la taille, la somme MD5, les permissions, le
propriétaire et le groupe de chaque fichier. Toutes les discordances
sont affichées. Les fichiers qui n\'avaient pas été installés à partir
du paquetage (p.ex. les fichiers de documentation exclus lors de
l\'installation en utilisant l\'option « **\--excludedocs** », seront
ignorés silencieusement.

Les options de sélection de paquetage sont les mêmes que celles
relatives à l\'interrogation de paquetages (ce qui inclut les fichiers
de manifeste de paquetage comme arguments). Les autres options ne
pouvant être utilisées qu\'en mode vérification sont :

**\--nodeps**

:   Ne pas vérifier les dépendances.

**\--nofiles**

:   Ne pas vérifier les fichiers.

**\--nomd5**

:   Ne pas vérifier les sommes de contrôle MD5.

**\--noscripts**

:   Ne pas exécuter le scriptlet **%verifyscript** (s\'il y en a un).

Le format de sortie est une chaîne de 9 caractères, un « **c** »
éventuel dénotant un fichier de configuration, et ensuite le nom du
fichier. Chacun des 9 caractères indique le résultat d\'une comparaison
d\'attribut(s) du fichier avec la valeur du (des) attribut(s)
enregistré(s) dans la base de données. Un « **.** » (point) seul
signifie que le test s\'est bien passé, alors qu\'un « **?** » seul
indique que le test n\'a pas pu être effectué (p.ex. quand les
permissions d\'accès aux fichier empêchent la lecture). Sinon, le
caractère mnémonique affiché en **G**ras dénote l\'échec du test
**\--verify** correspondant :

**S** la taille (**S**ize) du fichier diffère

**M** le **M**ode diffère (inclut les permissions et le type du fichier)

**5** la somme MD**5** diffère

**D** Le numéro de périphérique (**D**evice) majeur/mineur diffère

**L** Le chemin renvoyé par read**L**ink(2) diffère

**U** L\'**U**tilisateur propriétaire diffère

**G** Le **G**roupe propriétaire diffère

**T** La date de dernière modification (m**T**ime) diffère

VÉRIFICATION DE SIGNATURE
-------------------------

La forme générale d\'une commande de vérification de signature rpm est

**rpm** **\--checksig** \[**\--nogpg**\] \[**\--nopgp**\]
\[**\--nomd5**\] *FICHIER\_PAQUETAGE \...*

Ceci vérifie la signature PGP du paquetage *\<fichier\_paquetage\>* pour
s\'assurer de son intégrité et de son origine. Les informations de
configuration PGP sont lues à partir des fichiers de configuration.
Voyez la section sur les SIGNATURES PGP pour les détails.

OPTIONS DE DÉSINSTALLATION
--------------------------

La forme générale d\'une commande de désinstalltion rpm est

**rpm** {**-e\|\--erase**} \[**\--allmatches**\] \[**\--nodeps**\]
\[**\--noscripts**\] \[**\--notriggers**\] \[**\--test**\]
*NOM\_PAQUETAGE \...*

Les options suivantes peuvent également être utilisées :

**\--allmatches**

:   Désinstaller toutes les versions du paquetage correspondant à
    *NOM\_PAQUETAGE*. Normalement, une erreur se produit si
    *NOM\_PAQUETAGE* correspond à plusieurs paquetages.

**\--nodeps**

:   Ne pas effectuer de vérification des dépendances avant de
    désinstaller les paquetages.

**\--noscripts**

:   

**\--nopreun**

:   

**\--nopostun**

:   Ne pas exécuter le scriptlet de même nom. L\'option **\--noscripts**
    lors de la désinstallation du paquetage est équivalente à

**\--nopreun** **\--nopostun**

et désactive l\'exécution du ou des scriptlets **%preun** et **%postun**
correspondants.

**\--notriggers**

:   

**\--notriggerun**

:   

**\--notriggerpostun**

:   Ne pas exécuter de scriptlet déclenché du type spécifié. L\'option
    **\--notriggers** est équivalente à

**\--notriggerun** **\--notriggerpostun**

et désactive l\'exécution du ou des scriptlets **%triggerun** et
**%triggerpostun** correspondants.

**\--test**

:   Ne pas réellement désinstaller quoi que ce soit, simplement
    effectuer un test pour voir si c\'est possible. Utile conjointement
    avec l\'option **-vv** pour le débogage.

OPTIONS DE CONSTRUCTION
-----------------------

La forme générale d\'une commande de construction rpm est

**rpm** {**-b***ÉTAPE***\|-t***ÉTAPE*} \[**options-construction**\]
*FICHIER \...*

L\'argument utilisé est **-b** si un fichier spec est utilisé pour
construire le paquetage et **-t** si **rpm** devrait examiner le contenu
d\'un fichier tar (éventuellement compressé) pour obtenir le fichier de
spécifications à utiliser. Après le premier argument, le caractère
suivant (*ÉTAPE*) spécifie les étapes de construction et d\'empaquetage
à effectuer, et peut être :

**-ba**

:   Construire les paquetages binaires et sources (après avoir effectué
    les étapes %prep, %build et %install).

**-bb**

:   Construire un paquetage binaire (après avoir effectué les étapes
    %prep, %build et %install).

**-bp**

:   Exécuter l\'étape « %prep » du fichier de spécifications.
    Normalement, ceci implique de dépaqueter les sources et d\'appliquer
    tous les patches.

**-bc**

:   Effectuer l\'étape « %build » du fichier de spécifications (après
    avoir effectué l\'étape %prep). Cela implique en général
    l\'équivalent d\'un « make ».

**-bi**

:   Effectuer l\'étape « %install » du fichier de spécifications (après
    avoir effectué les étapes %prep et %build). Cela implique
    généralement l\'équivalent d\'un « make install ».

**-bl**

:   Accomplir une « vérification de liste ». La section « %files » du
    fichier de spécifications subit le développement des macros, et des
    vérifications sont effectuées pour vérifier que chaque fichier
    existe.

**-bs**

:   Construire uniquement le paquetage de sources.

Les options suivantes peuvent également être utilisées :

**\--buildroot ***RÉPERTOIRE*

:   Lors de la construction du paquetage, surcharger l\'étiquette
    BuildRoot (Construire Racine) avec le répertoire *RÉPERTOIRE*.

**\--clean**

:   Supprimer l\'arbre de construction après que les paquetages aient
    été créés.

**\--nobuild**

:   N\'exécuter aucune étape de construction. Utile pour le test de
    fichiers spec.

**\--rmsource**

:   Supprimer les sources après la construction (cette option peut
    également être utilisée seule ; exemple : « **rpm \--rmsource
    foo.spec** »).

**\--rmspec**

:   Supprimer le fichier spec après la construction (peut également être
    utilisé seul, p.ex. « **rpm \--rmspec foo.spec** »).

**\--short-circuit**

:   Aller directement à l\'étape spécifiée (c.-à-d. éviter toutes les
    étapes intermédiaires). Uniquement valide avec **-bc** et **-bi**.

**\--sign**

:   Incorporer une signature PGP dans le paquetage. Cette signature peut
    être utilisée pour vérifier l\'intégrité et l\'origine du paquetage.
    Voyez la section sur les SIGNATURES PGP pour les détails de
    configuration.

**\--target ***PLATE-FORME*

:   Pendant la construction du paquetage, interpréter *PLATE-FORME*
    comme étant la valeur de **arch-vendor-os** et fixer les macros
    **%\_target**, **%\_target\_arch** et **%\_target\_os** en
    conséquence.

OPTIONS DE RECONSTRUCTION ET DE RECOMPILATION
---------------------------------------------

Il y a deux autres façons d\'invoquer une construction avec rpm :

**rpm** {**\--rebuild\|\--recompile**} *PAQUETAGE\_SOURCE \...*

Quand il est invoqué de cette façon, **rpm** installe le paquetage de
sources désigné, et effectue une préparation, une compilation et une
installation. **\--rebuild** construit en outre un nouveau paquetage
binaire. Quand la construction est terminée, le répertoire de
construction est supprimé (comme avec **\--clean**) et les sources ainsi
que le fichier de spécifications du paquetage sont supprimés.

SIGNER UN PAQUETAGE
-------------------

**rpm** {**\--addsign\|\--resign**} *FICHIER\_PAQUETAGE \...*

L\'option **\--addsign** génère et insère de nouvelles signatures pour
chaque paquetage. Toute signature existante sera supprimée.

L\'option **\--resign** génère et ajoute les nouvelles signatures pour
les paquetages spécifiés tous en conservant celles existant déjà.

SIGNATURES GPG
--------------

Pour utiliser la fonctionnalité de signature, **rpm** doit être
configuré pour exécuter GPG, et doit être capable de trouver un
porte-clés public (keyring) comportant les clés publiques de Red Hat (ou
d\'un autre vendeur). Par défaut, **rpm** utilise les mêmes conventions
que GPG pour trouver les porte-clés, à savoir la variable
d\'environnement **\$GPGPATH**). Si vos porte-clés ne sont pas situés là
où GPG les attend, vous devrez fixer la valeur de la macro
**%\_gpg\_path** à l\'endroit où se situent les porte-clés GPG à
utiliser.

Si vous voulez pouvoir signer les paquetages que vous avez créés
vous-même, vous devrez également créer votre propre paire clé
publique/clé secrète (voir le manuel GPG). Vous devrez également
configurer les macros suivantes :

**%\_gpg\_name**

:   Le nom de l\'« utilisateur » dont vous voulez utiliser la clé pour
    signer vos paquetages.

Lors de la construction de paquetages, vous ajouterez ensuite
**\--sign** sur la ligne de commandes. On vous demandera votre phrase de
passe, et votre paquetage sera construit et signé. Par exemple, pour
pouvoir utiliser GPG pour signer les paquetages en tant qu\'utilisateur
*« John Doe* \<jdoe\@foo.com\> » à partir des porte-clés situés dans
**/etc/rpm/.gpg** en utilisant l\'exécutable **/usr/bin/gpg**, vous
devriez inclure

    %_gpg_path /etc/rpm/.gpg
    %_gpg_name John Doe <jdoe@foo.com>
    %_gpgbin /usr/bin/gpg

dans un fichier de configuration de macros. Utilisez **/etc/rpm/macros**
pour une configuration par système et **\~/.rpmmacros** pour une
configuration par utilisateur.

OPTIONS DE RECONSTRUCTION DE BASE DE DONNÉES
--------------------------------------------

La forme générale d\'une commande de reconstruction d\'une base de
données rpm est

**rpm** {**\--initdb\|\--rebuilddb**} \[**-v**\] \[**\--dbpath
***RÉPERTOIRE*\] \[**\--root ***RÉPERTOIRE*\]

Utilisez **\--initdb** pour reconstruire une nouvelle base de données ;
utilisez **\--rebuilddb** pour reconstruire les index de la base de
données à partir des en-têtes des paquetages installés.

SHOWRC
------

La commande

**rpm** **\--showrc**

affiche les valeurs que **rpm** va utiliser pour toutes les options qui
sont actuellement définies dans le(s) fichier(s) de configuration
*rpmrc* et *macros*.

OPTIONS FTP/HTTP
----------------

**rpm** peut agir comme un client FTP et/ou HTTP afin que les paquetages
puissent être interrogés et installés à partir d\'Internet. Les fichiers
de paquetage pour les opérations d\'installation, de mise à niveau et
d\'interrogation peuvent être spécifiés dans une URL de style **ftp** ou
**http** :

ftp://UTILISATEUR:MOT-PASSE\@HÔTE:PORT/chemin/vers/paquetage.rpm

Si la partie **:MOT-PASSE** est omise, le mot de passe sera demandé (une
seule fois par paire utilisateur/nom\_hôte). Si tant l\'utilisateur que
le mot de passe est omis, le **ftp** anonyme est utilisé. Dans tous les
cas, des transferts **ftp** passifs (PASV) sont effectués.

**rpm** permet d\'utiliser les options suivantes avec les URLs ftp :

**\--ftpproxy ***HÔTE*

:   L\'hôte *HÔTE* sera utilisé comme serveur proxy pour tous les
    transferts ftp, ce qui permet aux utilisateurs d\'effectuer des
    connexions ftp au travers de firewalls (gardes-barrières) qui
    utilisent des proxys. Cette option peut également être spécifiée en
    configurant la macro **%\_ftpproxy**.

**\--ftpport ***PORT*

:   Le numéro de *PORT* TCP à utiliser pour la connexion ftp sur le
    serveur proxy ftp au lieu du port par défaut. Cette option peut
    également être spécifiée en configurant la macro **%\_ftpport**.

**rpm** permet d\'utiliser les options suivantes avec les URL **http** :

**\--httpproxy ***HÔTE*

:   L\'hôte *HÔTE* sera utilisé comme un serveur délégué (proxy) pour
    tous les transferts **http**. Cette option peut également être
    spécifiée en configurant la macro **%\_httpproxy**.

**\--httpport ***PORT*

:   Le numéro de *PORT* TCP à utiliser pour la connexion **http** sur le
    serveur proxy http au lieu du port par défaut. Cette option peut
    également être spécifiée en configurant la macro **%\_httpport**.

FICHIERS
========

*/usr/lib/rpm/rpmrc*

*/etc/rpmrc*

*\~/.rpmrc*

*/usr/lib/rpm/macros*

*/etc/rpm/macros*

*\~/.rpmmacros*

*/var/lib/rpm/Conflictname*

*/var/lib/rpm/Basenames*

*/var/lib/rpm/Group*

*/var/lib/rpm/Name*

*/var/lib/rpm/Packages*

*/var/lib/rpm/Providename*

*/var/lib/rpm/Requirename*

*/var/lib/rpm/Triggername*

*/var/tmp/rpm\**

VOIR AUSSI
==========

**popt**(3),

**rpm2cpio**(8),

**rpmbuild**(8),

**http://www.rpm.org/**

AUTEURS
=======

Marc Ewing \<marc\@redhat.com\>

Jeff Johnson \<jbj\@redhat.com\>

Erik Troan \<ewt\@redhat.com\>

TRADUCTION
==========

Frédéric Delanoy \<*delanoy\_f at yahoo.com*\>, 2002.
