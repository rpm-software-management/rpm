---
date: 09 June 2002
section: 8
title: RPM
---

NAME
====

rpm - RPM Package Manager

SYNOPSIS
========

QUERYING AND VERIFYING PACKAGES:
--------------------------------

**rpm** {**-q\|\--query**} \[**select-options**\] \[**query-options**\]

**rpm** **\--querytags**

**rpm** {**-V\|\--verify**} \[**select-options**\]
\[**verify-options**\]

INSTALLING, UPGRADING, AND REMOVING PACKAGES:
---------------------------------------------

**rpm** {**-i\|\--install**} \[**install-options**\] *PACKAGE\_FILE
\...*

**rpm** {**-U\|\--upgrade**} \[**install-options**\] *PACKAGE\_FILE
\...*

**rpm** {**-F\|\--freshen**} \[**install-options**\] *PACKAGE\_FILE
\...*

**rpm** {**\--reinstall**} \[**install-options**\] *PACKAGE\_FILE \...*

**rpm** {**-e\|\--erase**} \[**\--allmatches**\] \[**\--justdb\]
\[\--nodeps**\] \[**\--noscripts**\] \[**\--notriggers**\]
\[**\--test**\] *PACKAGE\_NAME \...*

MISCELLANEOUS:
--------------

**rpm** **\--showrc**

**rpm** **\--setperms** *PACKAGE\_NAME \...*

**rpm** **\--setugids** *PACKAGE\_NAME \...*

**rpm** **\--setcaps** *PACKAGE\_NAME \...*

**rpm** **\--restore** *PACKAGE\_NAME \...*

select-options
--------------

\[*PACKAGE\_NAME*\] \[**-a,\--all \[***SELECTOR*\]\] \[**-f,\--file
***FILE*\] \[**-g,\--group ***GROUP*\] \[**-p,\--package
***PACKAGE\_FILE*\] \[**\--hdrid ***SHA1*\] \[**\--pkgid ***MD5*\]
\[**\--tid ***TID*\] \[**\--querybynumber ***HDRNUM*\]
\[**\--triggeredby ***PACKAGE\_NAME*\] \[**\--whatprovides
***CAPABILITY*\] \[**\--whatrequires ***CAPABILITY*\]
\[**\--whatrecommends ***CAPABILITY*\] \[**\--whatsuggests
***CAPABILITY*\] \[**\--whatsupplements ***CAPABILITY*\]
\[**\--whatenhances ***CAPABILITY*\] \[**\--whatobsoletes
***CAPABILITY*\] \[**\--whatconflicts ***CAPABILITY*\]

query-options
-------------

General: \[**\--changelog**\] \[**\--changes**\] \[**\--dupes**\]
\[**-i,\--info**\] \[**\--last**\] \[**\--qf,\--queryformat
***QUERYFMT*\] \[**\--xml**\]

Dependencies: \[**\--conflicts**\] \[**\--enhances**\]
\[**\--obsoletes**\] \[**\--provides**\] \[**\--recommends**\]
\[**-R,\--requires**\] \[**\--suggests**\] \[**\--supplements**\]

Files: \[**-c,\--configfiles**\] \[**-d,\--docfiles**\] \[**\--dump**\]
\[**\--fileclass**\] \[**\--filecolor**\]
\[**\--fileprovide**\]\[**\--filerequire**\] \[**\--filecaps**\]
\[**\--filesbypkg**\] \[**-l,\--list**\] \[**-s,\--state**\]
\[**\--noartifact**\] \[**\--noghost**\] \[**\--noconfig**\]

Scripts and triggers: \[**\--filetriggers**\] \[**\--scripts**\]
\[**\--triggers,\--triggerscripts**\]

verify-options
--------------

\[**\--nodeps**\] \[**\--nofiles**\] \[**\--noscripts**\]
\[**\--nodigest**\] \[**\--nosignature**\] \[**\--nolinkto**\]
\[**\--nofiledigest**\] \[**\--nosize**\] \[**\--nouser**\]
\[**\--nogroup**\] \[**\--nomtime**\] \[**\--nomode**\]
\[**\--nordev**\] \[**\--nocaps**\]

install-options
---------------

\[**\--allfiles**\] \[**\--badreloc**\] \[**\--excludepath ***OLDPATH*\]
\[**\--excludedocs**\] \[**\--force**\] \[**-h,\--hash**\]
\[**\--ignoresize**\] \[**\--ignorearch**\] \[**\--ignoreos**\]
\[**\--includedocs**\] \[**\--justdb**\] \[**\--nodeps**\]
\[**\--nodigest**\] \[**\--noplugins**\] \[**\--nocaps**\]
\[**\--noorder**\] \[**\--noverify**\] \[**\--nosignature**\]
\[**\--noscripts**\] \[**\--notriggers**\] \[**\--oldpackage**\]
\[**\--percent**\] \[**\--prefix ***NEWPATH*\] \[**\--relocate
***OLDPATH***=***NEWPATH*\] \[**\--replacefiles**\]
\[**\--replacepkgs**\] \[**\--test**\]

DESCRIPTION
===========

**rpm** is a powerful **Package Manager**, which can be used to build,
install, query, verify, update, and erase individual software packages.
A **package** consists of an archive of files and meta-data used to
install and erase the archive files. The meta-data includes helper
scripts, file attributes, and descriptive information about the package.
**Packages** come in two varieties: binary packages, used to encapsulate
software to be installed, and source packages, containing the source
code and recipe necessary to produce binary packages.

One of the following basic modes must be selected: **Query**,
**Verify**, **Install/Upgrade/Freshen/Reinstall**, **Uninstall**, **Set
Owners/Groups**, **Show Querytags**, and **Show Configuration**.

GENERAL OPTIONS
---------------

These options can be used in all the different modes.

**-?, \--help**

:   Print a longer usage message than normal.

**\--version**

:   Print a single line containing the version number of **rpm** being
    used.

**\--quiet**

:   Print as little as possible - normally only error messages will be
    displayed.

**-v, \--verbose**

:   Print verbose information - normally routine progress messages will
    be displayed.

**-vv**

:   Print lots of ugly debugging information.

**\--rcfile ***FILELIST*

:   Replace the list of configuration files to be read. Each of the
    files in the colon separated *FILELIST* is read sequentially by
    **rpm** for configuration information. Only the first file in the
    list must exist, and tildes will be expanded to the value of
    **\$HOME**. The default *FILELIST* is
    */usr/lib/rpm/rpmrc*:*/usr/lib/rpm/redhat/rpmrc*:*/etc/rpmrc*:*\~/.rpmrc*.

```{=html}
<!-- -->
```

**\--load ***FILE*

:   Load an individual macro file.

```{=html}
<!-- -->
```

**\--macros ***FILELIST*

:   Replace the list of macro files to be loaded. Each of the files in
    the colon separated *FILELIST* is read sequentially by **rpm** for
    macro definitions. Only the first file in the list must exist, and
    tildes will be expanded to the value of **\$HOME**. The default
    *FILELIST* is
    */usr/lib/rpm/macros*:*/usr/lib/rpm/macros.d/macros.\**:*/usr/lib/rpm/platform/%{\_target}/macros*:*/usr/lib/rpm/fileattrs/\*.attr*:*/usr/lib/rpm/redhat/macros*:*/etc/rpm/macros.\**:*/etc/rpm/macros*:*/etc/rpm/%{\_target}/macros*:*\~/.rpmmacros*

```{=html}
<!-- -->
```

**\--pipe ***CMD*

:   Pipes the output of **rpm** to the command *CMD*.

**\--dbpath ***DIRECTORY*

:   Use the database in *DIRECTORY* rather than the default path
    */var/lib/rpm*

**\--root ***DIRECTORY*

:   Use the file system tree rooted at *DIRECTORY* for all operations.
    Note that this means the database within *DIRECTORY* will be used
    for dependency checks and any scriptlet(s) (e.g. **%post** if
    installing, or **%prep** if building, a package) will be run after a
    chroot(2) to *DIRECTORY*.

**-D, \--define=\'***MACRO EXPR***\'**

:   Defines *MACRO* with value *EXPR*.

**\--undefine=\'***MACRO***\'**

:   Undefines *MACRO*.

**-E, \--eval=\'***EXPR***\'**

:   Prints macro expansion of *EXPR*.

More - less often needed - options can be found on the **rpm-misc(8)**
man page.

INSTALL AND UPGRADE OPTIONS
---------------------------

In these options, *PACKAGE\_FILE* can be either **rpm** binary file or
ASCII package manifest (see **PACKAGE SELECTION OPTIONS**), and may be
specified as an **ftp** or **http** URL, in which case the package will
be downloaded before being installed. See **FTP/HTTP OPTIONS** for
information on **rpm**\'s **ftp** and **http** client support.

The general form of an rpm install command is

**rpm** {**-i\|\--install**} \[**install-options**\] *PACKAGE\_FILE
\...*

This installs a new package.

The general form of an rpm upgrade command is

**rpm** {**-U\|\--upgrade**} \[**install-options**\] *PACKAGE\_FILE
\...*

This upgrades or installs the package currently installed to a newer
version. This is the same as install, except all other version(s) of the
package are removed after the new package is installed.

**rpm** {**-F\|\--freshen**} \[**install-options**\] *PACKAGE\_FILE
\...*

This will upgrade packages, but only ones for which an earlier version
is installed.

The general form of an rpm reinstall command is

**rpm** {**\--reinstall**} \[**install-options**\] *PACKAGE\_FILE \...*

This reinstalls a previously installed package.

**\--allfiles**

:   Installs or upgrades all the missingok files in the package,
    regardless if they exist.

**\--badreloc**

:   Used with **\--relocate**, permit relocations on all file paths, not
    just those *OLDPATH*\'s included in the binary package relocation
    hint(s).

**\--excludepath ***OLDPATH*

:   Don\'t install files whose name begins with *OLDPATH*.

**\--excludeartifacts**

:   Don\'t install any files which are marked as artifacts, such as
    build-id links.

**\--excludedocs**

:   Don\'t install any files which are marked as documentation (which
    includes man pages and texinfo documents).

**\--force**

:   Same as using **\--replacepkgs**, **\--replacefiles**, and
    **\--oldpackage**.

**-h, \--hash**

:   Print 50 hash marks as the package archive is unpacked. Use with
    **-v\|\--verbose** for a nicer display.

**\--ignoresize**

:   Don\'t check mount file systems for sufficient disk space before
    installing this package.

**\--ignorearch**

:   Allow installation or upgrading even if the architectures of the
    binary package and host don\'t match.

**\--ignoreos**

:   Allow installation or upgrading even if the operating systems of the
    binary package and host don\'t match.

**\--includedocs**

:   Install documentation files. This is the default behavior.

**\--justdb**

:   Update only the database, not the filesystem.

**\--nodigest**

:   Don\'t verify package or header digests when reading.

**\--nomanifest**

:   Don\'t process non-package files as manifests.

**\--nosignature**

:   Don\'t verify package or header signatures when reading.

**\--nodeps**

:   Don\'t do a dependency check before installing or upgrading a
    package.

**\--nocaps**

:   Don\'t set file capabilities.

**\--noorder**

:   Don\'t reorder the packages for an install. The list of packages
    would normally be reordered to satisfy dependencies.

**\--noverify**

:   Don\'t perform verify package files prior to installation.

**\--noplugins**

:   Do not load and execute plugins.

**\--noscripts**, **\--nopre**, **\--nopost**, **\--nopreun**, **\--nopostun**, **\--nopretrans**, **\--noposttrans**

:   Don\'t execute the scriptlet of the same name. The **\--noscripts**
    option is equivalent to

**\--nopre** **\--nopost** **\--nopreun** **\--nopostun**
**\--nopretrans** **\--noposttrans**

and turns off the execution of the corresponding **%pre**, **%post**,
**%preun**, **%postun** **%pretrans**, and **%posttrans** scriptlet(s).

**\--notriggers**, **\--notriggerin**, **\--notriggerun**, **\--notriggerprein**, **\--notriggerpostun**

:   Don\'t execute any trigger scriptlet of the named type. The
    **\--notriggers** option is equivalent to

**\--notriggerprein** **\--notriggerin** **\--notriggerun**
**\--notriggerpostun**

and turns off execution of the corresponding **%triggerprein**,
**%triggerin**, **%triggerun**, and **%triggerpostun** scriptlet(s).

**\--oldpackage**

:   Allow an upgrade to replace a newer package with an older one.

**\--percent**

:   Print percentages as files are unpacked from the package archive.
    This is intended to make **rpm** easy to run from other tools.

**\--prefix ***NEWPATH*

:   For relocatable binary packages, translate all file paths that start
    with the installation prefix in the package relocation hint(s) to
    *NEWPATH*.

**\--relocate ***OLDPATH***=***NEWPATH*

:   For relocatable binary packages, translate all file paths that start
    with *OLDPATH* in the package relocation hint(s) to *NEWPATH*. This
    option can be used repeatedly if several *OLDPATH*\'s in the package
    are to be relocated.

**\--replacefiles**

:   Install the packages even if they replace files from other, already
    installed, packages.

**\--replacepkgs**

:   Install the packages even if some of them are already installed on
    this system.

**\--test**

:   Do not install the package, simply check for and report potential
    conflicts.

ERASE OPTIONS
-------------

The general form of an rpm erase command is

**rpm** {**-e\|\--erase**} \[**\--allmatches**\] \[**\--justdb\]
\[\--nodeps**\] \[**\--noscripts**\] \[**\--notriggers**\]
\[**\--test**\] *PACKAGE\_NAME \...*

The following options may also be used:

**\--allmatches**

:   Remove all versions of the package which match *PACKAGE\_NAME*.
    Normally an error is issued if *PACKAGE\_NAME* matches multiple
    packages.

**\--justdb**

:   Update only the database, not the filesystem.

**\--nodeps**

:   Don\'t check dependencies before uninstalling the packages.

**\--noscripts**, **\--nopreun**, **\--nopostun**

:   Don\'t execute the scriptlet of the same name. The **\--noscripts**
    option during package erase is equivalent to

**\--nopreun** **\--nopostun**

and turns off the execution of the corresponding **%preun**, and
**%postun** scriptlet(s).

**\--notriggers**, **\--notriggerun**, **\--notriggerpostun**

:   Don\'t execute any trigger scriptlet of the named type. The
    **\--notriggers** option is equivalent to

**\--notriggerun** **\--notriggerpostun**

and turns off execution of the corresponding **%triggerun**, and
**%triggerpostun** scriptlet(s).

**\--test**

:   Don\'t really uninstall anything, just go through the motions.
    Useful in conjunction with the **-vv** option for debugging.

QUERY OPTIONS
-------------

The general form of an rpm query command is

**rpm** {**-q\|\--query**} \[**select-options**\] \[**query-options**\]

You may specify the format that package information should be printed
in. To do this, you use the

**\--qf\|\--queryformat** *QUERYFMT*

option, followed by the *QUERYFMT* format string. Query formats are
modified versions of the standard **printf(3)** formatting. The format
is made up of static strings (which may include standard C character
escapes for newlines, tabs, and other special characters) and
**printf(3)** type formatters. As **rpm** already knows the type to
print, the type specifier must be omitted however, and replaced by the
name of the header tag to be printed, enclosed by **{}** characters. Tag
names are case insensitive, and the leading **RPMTAG\_** portion of the
tag name may be omitted as well.

Alternate output formats may be requested by following the tag with
**:***typetag*. Currently, the following types are supported:

**:armor**

:   Wrap a public key in ASCII armor.

**:arraysize**

:   Display number of elements in array tags.

**:base64**

:   Encode binary data using base64.

**:date**

:   Use strftime(3) \"%c\" format.

**:day**

:   Use strftime(3) \"%a %b %d %Y\" format.

**:depflags**

:   Format dependency comparison operator.

**:deptype**

:   Format dependency type.

**:expand**

:   Perform macro expansion.

**:fflags**

:   Format file flags.

**:fstate**

:   Format file state.

**:fstatus**

:   Format file verify status.

**:hex**

:   Format in hexadecimal.

**:octal**

:   Format in octal.

**:humaniec**

:   Human readable number (in IEC 80000). The suffix K = 1024, M =
    1048576, \...

**:humansi**

:   Human readable number (in SI). The suffix K = 1000, M = 1000000,
    \...

**:perms**

:   Format file permissions.

**:pgpsig**

:   Display signature fingerprint and time.

**:shescape**

:   Escape single quotes for use in a script.

**:string**

:   Display string format. (default)

**:tagname**

:   Display tag name.

**:tagnum**

:   Display tag number.

**:triggertype**

:   Display trigger suffix.

**:vflags**

:   File verification flags.

**:xml**

:   Wrap data in simple xml markup.

For example, to print only the names of the packages queried, you could
use **%{NAME}** as the format string. To print the packages name and
distribution information in two columns, you could use
**%-30{NAME}%{DISTRIBUTION}**. **rpm** will print a list of all of the
tags it knows about when it is invoked with the **\--querytags**
argument.

There are three subsets of options for querying: package selection, file
selection and information selection.

PACKAGE SELECTION OPTIONS:
--------------------------

*PACKAGE\_NAME*

:   Query installed package named *PACKAGE\_NAME*. To specify the
    package more precisely the package name may be followed by the
    version or version and release both separated by a dash or an
    architecture name separated by a dot. See the output of **rpm -qa**
    or **rpm -qp ***PACKAGE\_FILE* as an example.

```{=html}
<!-- -->
```

**-a, \--all \[***SELECTOR*\]

:   Query all installed packages.

An optional *SELECTOR* in the form of tag=pattern can be provided to
narrow the selection, for example name=\"b\*\" to query packages whose
name starts with \"b\".

**\--dupes**

:   List duplicated packages.

**-f, \--file ***FILE*

:   Query package owning *FILE*.

**\--filecaps**

:   List file names with POSIX1.e capabilities.

**\--fileclass**

:   List file names with their classes (libmagic classification).

**\--filecolor**

:   List file names with their colors (0 for noarch, 1 for 32bit, 2 for
    64 bit).

**\--fileprovide**

:   List file names with their provides.

**\--filerequire**

:   List file names with their requires.

**-g, \--group ***GROUP*

:   Query packages with the group of *GROUP*.

**\--hdrid ***SHA1*

:   Query package that contains a given header identifier, i.e. the
    *SHA1* digest of the immutable header region.

**-p, \--package ***PACKAGE\_FILE*

:   Query an (uninstalled) package *PACKAGE\_FILE*. The *PACKAGE\_FILE*
    may be specified as an **ftp** or **http** style URL, in which case
    the package header will be downloaded and queried. See **FTP/HTTP
    OPTIONS** for information on **rpm**\'s **ftp** and **http** client
    support. The *PACKAGE\_FILE* argument(s), if not a binary package,
    will be interpreted as an ASCII package manifest unless
    **\--nomanifest** option is used. In manifests, comments are
    permitted, starting with a \'\#\', and each line of a package
    manifest file may include white space separated glob expressions,
    including URL\'s, that will be expanded to paths that are
    substituted in place of the package manifest as additional
    *PACKAGE\_FILE* arguments to the query.

**\--pkgid ***MD5*

:   Query package that contains a given package identifier, i.e. the
    *MD5* digest of the combined header and payload contents.

**\--querybynumber ***HDRNUM*

:   Query the *HDRNUM*th database entry directly; this is useful only
    for debugging.

**\--specfile ***SPECFILE*

:   Parse and query *SPECFILE* as if it were a package. Although not all
    the information (e.g. file lists) is available, this type of query
    permits rpm to be used to extract information from spec files
    without having to write a specfile parser.

**\--tid ***TID*

:   Query package(s) that have a given *TID* transaction identifier. A
    unix time stamp is currently used as a transaction identifier. All
    package(s) installed or erased within a single transaction have a
    common identifier.

**\--triggeredby ***PACKAGE\_NAME*

:   Query packages that are triggered by package(s) *PACKAGE\_NAME*.

**\--whatobsoletes ***CAPABILITY*

:   Query all packages that obsolete *CAPABILITY* for proper
    functioning.

**\--whatprovides ***CAPABILITY*

:   Query all packages that provide the *CAPABILITY* capability.

**\--whatrequires ***CAPABILITY*

:   Query all packages that require *CAPABILITY* for proper functioning.

**\--whatconflicts ***CAPABILITY*

:   Query all packages that conflict with *CAPABILITY*.

**\--whatrecommends ***CAPABILITY*

:   Query all packages that recommend *CAPABILITY*.

**\--whatsuggests ***CAPABILITY*

:   Query all packages that suggest *CAPABILITY*.

**\--whatsupplements ***CAPABILITY*

:   Query all packages that supplement *CAPABILITY*.

**\--whatenhances ***CAPABILITY*

:   Query all packages that enhance *CAPABILITY*.

PACKAGE QUERY OPTIONS:
----------------------

**\--changelog**

:   Display change information for the package.

**\--changes**

:   Display change information for the package with full time stamps.

**\--conflicts**

:   List capabilities this package conflicts with.

**\--dump**

:   Dump file information as follows (implies **-l**):

>     path size mtime digest mode owner group isconfig isdoc rdev symlink
>     	

**\--enhances**

:   List capabilities enhanced by package(s)

**\--filesbypkg**

:   List all the files in each selected package.

**\--filetriggers**

:   List filetrigger scriptlets from package(s).

**-i, \--info**

:   Display package information, including name, version, and
    description. This uses the **\--queryformat** if one was specified.

**\--last**

:   Orders the package listing by install time such that the latest
    packages are at the top.

**-l, \--list**

:   List files in package.

**\--obsoletes**

:   List packages this package obsoletes.

**\--provides**

:   List capabilities this package provides.

**\--recommends**

:   List capabilities recommended by package(s)

**-R, \--requires**

:   List capabilities on which this package depends.

**\--suggests**

:   List capabilities suggested by package(s)

**\--supplements**

:   List capabilities supplemented by package(s)

**\--scripts**

:   List the package specific scriptlet(s) that are used as part of the
    installation and uninstallation processes.

**-s, \--state**

:   Display the *states* of files in the package (implies **-l**). The
    state of each file is one of *normal*, *not installed*, or
    *replaced*.

**\--triggers, \--triggerscripts**

:   Display the trigger scripts, if any, which are contained in the
    package. **\--xml** Format package headers as XML.

FILE SELECTION OPTIONS:
-----------------------

**-A, \--artifactfiles**

:   Only include artifact files (implies **-l**).

**-c, \--configfiles**

:   Only include configuration files (implies **-l**).

**-d, \--docfiles**

:   Only include documentation files (implies **-l**).

**-L, \--licensefiles**

:   Only include license files (implies **-l**).

**\--noartifact**

:   Exclude artifact files.

**\--noconfig**

:   Exclude config files.

**\--noghost**

:   Exclude ghost files.

VERIFY OPTIONS
--------------

The general form of an rpm verify command is

**rpm** {**-V\|\--verify**} \[**select-options**\]
\[**verify-options**\]

Verifying a package compares information about the installed files in
the package with information about the files taken from the package
metadata stored in the rpm database. Among other things, verifying
compares the size, digest, permissions, type, owner and group of each
file. Any discrepancies are displayed. Files that were not installed
from the package, for example, documentation files excluded on
installation using the \"**\--excludedocs**\" option, will be silently
ignored.

The package and file selection options are the same as for package
querying (including package manifest files as arguments). Other options
unique to verify mode are:

**\--nodeps**

:   Don\'t verify dependencies of packages.

**\--nodigest**

:   Don\'t verify package or header digests when reading.

**\--nofiles**

:   Don\'t verify any attributes of package files.

**\--noscripts**

:   Don\'t execute the **%verifyscript** scriptlet (if any).

**\--nosignature**

:   Don\'t verify package or header signatures when reading.

**\--nolinkto**

:   

**\--nofiledigest** (formerly **\--nomd5**)

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

:   Don\'t verify the corresponding file attribute.

**\--nocaps**

:   Don\'t verify file capabilities.

The format of the output is a string of 9 characters, a possible
attribute marker:

    c %config configuration file.
    d %doc documentation file.
    g %ghost file (i.e. the file contents are not included in the package payload).
    l %license license file.
    r %readme readme file.

from the package header, followed by the file name. Each of the 9
characters denotes the result of a comparison of attribute(s) of the
file to the value of those attribute(s) recorded in the database. A
single \"**.**\" (period) means the test passed, while a single
\"**?**\" (question mark) indicates the test could not be performed
(e.g. file permissions prevent reading). Otherwise, the (mnemonically
em**B**oldened) character denotes failure of the corresponding
**\--verify** test:

    S file Size differs
    M Mode differs (includes permissions and file type)
    5 digest (formerly MD5 sum) differs
    D Device major/minor number mismatch
    L readLink(2) path mismatch
    U User ownership differs
    G Group ownership differs
    T mTime differs
    P caPabilities differ

MISCELLANEOUS COMMANDS
----------------------

**rpm** **\--showrc**

:   shows the values **rpm** will use for all of the options are
    currently set in *rpmrc* and *macros* configuration file(s).

**rpm** **\--setperms** *PACKAGE\_NAME*

:   sets permissions of files in the given package. Consider using
    **\--restore** instead.

**rpm** **\--setugids** *PACKAGE\_NAME*

:   sets user/group ownership of files in the given package. This
    command can change permissions and capabilities of files in that
    package. In most cases it is better to use **\--restore** instead.

**rpm** **\--setcaps** *PACKAGE\_NAME*

:   sets capabilities of files in the given package. Consider using
    **\--restore** instead.

**rpm** **\--restore** *PACKAGE\_NAME*

:   The option restores owner, group, permissions and capabilities of
    files in the given package.

Options **\--setperms**, **\--setugids**, **\--setcaps** and

:   **\--restore** are mutually exclusive.

FTP/HTTP OPTIONS
----------------

**rpm** can act as an FTP and/or HTTP client so that packages can be
queried or installed from the internet. Package files for install,
upgrade, and query operations may be specified as an **ftp** or **http**
style URL:

http://HOST\[:PORT\]/path/to/package.rpm

ftp://\[USER:PASSWORD\]\@HOST\[:PORT\]/path/to/package.rpm

If both the user and password are omitted, anonymous **ftp** is used.

**rpm** allows the following options to be used with ftp URLs:

**rpm** allows the following options to be used with

:   **http** and **ftp** URLs:

**\--httpproxy ***HOST*

:   The host *HOST* will be used as a proxy server for all **http** and
    **ftp** transfers. This option may also be specified by configuring
    the macro **%\_httpproxy**.

**\--httpport ***PORT*

:   The TCP *PORT* number to use for the **http** connection on the
    proxy http server instead of the default port. This option may also
    be specified by configuring the macro **%\_httpport**.

LEGACY ISSUES
=============

Executing rpmbuild
------------------

The build modes of rpm are now resident in the */usr/bin/rpmbuild*
executable. Install the package containing **rpmbuild** (usually
**rpm-build**) and see **rpmbuild**(8) for documentation of all the
**rpm** build modes.

FILES
=====

rpmrc Configuration
-------------------

    /usr/lib/rpm/rpmrc
    /usr/lib/rpm/<vendor>/rpmrc
    /etc/rpmrc
    ~/.rpmrc

Macro Configuration
-------------------

    /usr/lib/rpm/macros
    /usr/lib/rpm/<vendor>/macros
    /etc/rpm/macros
    ~/.rpmmacros

Database
--------

    /var/lib/rpm/Basenames
    /var/lib/rpm/Conflictname
    /var/lib/rpm/Dirnames
    /var/lib/rpm/Group
    /var/lib/rpm/Installtid
    /var/lib/rpm/Name
    /var/lib/rpm/Obsoletename
    /var/lib/rpm/Packages
    /var/lib/rpm/Providename
    /var/lib/rpm/Requirename
    /var/lib/rpm/Sha1header
    /var/lib/rpm/Sigmd5
    /var/lib/rpm/Triggername

Temporary
---------

*/var/tmp/rpm\**

SEE ALSO
========

    rpm-misc(8),
    popt(3),
    rpm2cpio(8),
    rpmbuild(8),
    rpmdb(8),
    rpmkeys(8),
    rpmsign(8),
    rpmspec(8),

**rpm \--help** - as rpm supports customizing the options via popt
aliases it\'s impossible to guarantee that what\'s described in the
manual matches what\'s available.

**http://www.rpm.org/ \<URL:http://www.rpm.org/\>**

AUTHORS
=======

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
