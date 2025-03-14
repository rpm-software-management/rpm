RPM(8)

# NAME

rpm - RPM Package Manager

# SYNOPSIS

## QUERYING AND VERIFYING PACKAGES

*rpm* {*-q*|*--query*} [select-options] [query-options] [_PACKAGE_FILE_ ...] [_PACKAGE_NAME_ ...]

*rpm* {*-V*|*--verify*} [select-options] [verify-options] [_PACKAGE_NAME_ ...]

## INSTALLING, UPGRADING, AND REMOVING PACKAGES

*rpm* {*-i*|*--install*} [install-options] _PACKAGE_FILE_ ...

*rpm* {*-U*|*--upgrade*} [install-options] _PACKAGE_FILE_ ...

*rpm* {*-F*|*--freshen*} [install-options] _PACKAGE_FILE_ ...

*rpm* {*--reinstall*} [install-options] _PACKAGE_FILE_ ...

*rpm* {*-e*|*--erase*} [erase-options] _PACKAGE_NAME_ ...

*rpm* *--restore* [select-options] _PACKAGE_NAME_ ...

## MISC OPERATIONS

*rpm* *--querytags*

*rpm* *--showrc*

# DESCRIPTION

*rpm* is a powerful *Package Manager*, which can be used to build,
install, query, verify, update, and erase individual software packages.
A *package* consists of an archive of files and meta-data used to
install and erase the archive files. The meta-data includes helper
scripts, file attributes, and descriptive information about the package.
*Packages* come in two varieties: binary packages, used to encapsulate
software to be installed, and source packages, containing the source
code and recipe necessary to produce binary packages.

One of the following basic modes must be selected: *Query*,
*Verify*, *Install/Upgrade/Freshen/Reinstall*, *Uninstall*, *Set
Owners/Groups*, *Show Querytags*, and *Show Configuration*.

# GENERAL OPTIONS

These options can be used in all the different modes.

*-?*, *--help*
	Print a longer usage message than normal.

*--version*
	Print a single line containing the version number of *rpm* being
	used.

*--quiet*
	Print as little as possible - normally only error messages will be
	displayed.

*-v*, *--verbose*
	Print verbose information - normally routine progress messages will
	be displayed.

*-vv*
	Print lots of ugly debugging information.

*--rcfile* _FILELIST_
	Replace the default list of configuration files to be read with
	_FILELIST_.  See *rpmrc Configuration* for details.

*--load* _FILE_
	Load an individual macro file.

*--macros* _FILELIST_
	Replace the list of macro files to be loaded with _FILELIST_.
	See *Macro Configuration* for details.

*--pipe* _CMD_
	Pipes the output of *rpm* to the command _CMD_.

*--dbpath* _DIRECTORY_
	Use the database in _DIRECTORY_ rather than the default path
	*/var/lib/rpm*.

*--root* _DIRECTORY_
	Use the file system tree rooted at _DIRECTORY_ for all operations.
	Note that this means the database within _DIRECTORY_ will be used
	for dependency checks and any scriptlet(s) (e.g. *%post* if
	installing, or *%prep* if building, a package) will be run after a
	chroot(2) to _DIRECTORY_.

	Note that rpm assumes the environment inside the root is set up by
	the caller, such as any mounts needed for the operation inside the
	root directory.

*-D*, *--define*='_MACRO_ _EXPR_'
	Defines _MACRO_ with value _EXPR_.

*--undefine*='_MACRO_'
	Undefines _MACRO_.

*-E*, *--eval*='_EXPR_'
	Prints macro expansion of _EXPR_.

More - less often needed - options can be found on the *rpm-misc*(8)
man page.

# INSTALL AND UPGRADE OPTIONS

In these options, _PACKAGE_FILE_ can be either *rpm* binary file or
ASCII package manifest (see *PACKAGE SELECTION OPTIONS*), and may be
specified as an *ftp* or *http* URL, in which case the package will
be downloaded before being installed. See *FTP/HTTP OPTIONS* for
information on *rpm*'s *ftp* and *http* client support.

The general form of an *rpm* install command is

*rpm* {*-i*|*--install*} [install-options] _PACKAGE_FILE_ ...

This installs a new package.

The general form of an *rpm* upgrade command is

*rpm* {*-U*|*--upgrade*} [install-options] _PACKAGE_FILE_ ...

This upgrades or installs the package currently installed to a newer
version. This is the same as install, except all other version(s) of the
package are removed after the new package is installed.

*rpm* {*-F*|*--freshen*} [install-options] _PACKAGE_FILE_ ...

This will upgrade packages, but only ones for which an earlier version
is installed.

The general form of an *rpm* reinstall command is

*rpm* {*--reinstall*} [install-options] _PACKAGE_FILE_ ...

This reinstalls a previously installed package.

*--allfiles*
	Installs or upgrades all the missingok files in the package,
	regardless if they exist.

*--badreloc*
	Used with *--relocate*, permit relocations on all file paths, not
	just those _OLDPATH_'s included in the binary package relocation
	hint(s).

*--excludepath* _OLDPATH_
	Don't install files whose name begins with _OLDPATH_.

*--excludeartifacts*
	Don't install any files which are marked as artifacts, such as
	build-id links.

*--excludedocs*
	Don't install any files which are marked as documentation (which
	includes man pages and texinfo documents).

*--force*
	Same as using *--replacepkgs*, *--replacefiles*, and
	*--oldpackage*.

*-h*, *--hash*
	Print 50 hash marks as the package archive is unpacked. Use with
	*-v*|*--verbose* for a nicer display.

*--ignoresize*
	Don't check mount file systems for sufficient disk space before
	installing this package.

*--ignorearch*
	Allow installation or upgrading even if the architectures of the
	binary package and host don't match.

*--ignoreos*
	Allow installation or upgrading even if the operating systems of the
	binary package and host don't match.

*--includedocs*
	Install documentation files. This is the default behavior.

*--justdb*
	Update only the database, not the filesystem.

*--nodb*
	Update only the filesystem, not the database.

*--nodigest*
	Don't verify package or header digests when reading.

*--nosignature*
	Don't verify package or header signatures when reading.

*--nodeps*
	Don't do a dependency check before installing or upgrading a
	package.

*--nocaps*
	Don't set file capabilities.

*--noorder*
	Don't reorder the packages for an install. The list of packages
	would normally be reordered to satisfy dependencies.

*--noverify*
	Don't perform verify package files prior to installation.

*--noplugins*
	Do not load and execute plugins.

*--noscripts*
	Disables execution of package scriptlets. Equivalent to

	*--nopre* *--nopost* *--nopreun* *--nopostun*
	*--nopretrans* *--noposttrans* *--nopreuntrans* *--nopostuntrans*

*--nopre*
*--nopost*
*--nopreun*
*--nopostun*
*--nopretrans*
*--noposttrans*
*--nopreuntrans*
*--nopostuntrans*
	Don't execute the scriptlets of the named type.

*--notriggers*
	Disable execution of package triggers. Equivalent to

	*--notriggerprein* *--notriggerin* *--notriggerun*
	*--notriggerpostun*

*--notriggerin*
*--notriggerun*
*--notriggerprein*
*--notriggerpostun*
	Disable execution of the trigger scriptlets of the named type.

*--nosysusers*
	Don't create sysusers from packages

*--oldpackage*
	Allow an upgrade to replace a newer package with an older one.

*--percent*
	Print percentages as files are unpacked from the package archive.
	This is intended to make *rpm* easy to run from other tools.

*--prefix* _NEWPATH_
	For relocatable binary packages, translate all file paths that start
	with the installation prefix in the package relocation hint(s) to
	_NEWPATH_.

*--relocate* _OLDPATH_=_NEWPATH_
	For relocatable binary packages, translate all file paths that start
	with _OLDPATH_ in the package relocation hint(s) to _NEWPATH_. This
	option can be used repeatedly if several _OLDPATH_'s in the package
	are to be relocated.

*--replacefiles*
	Install the packages even if they replace files from other, already
	installed, packages.

*--replacepkgs*
	Install the packages even if some of them are already installed on
	this system.

*--test*
	Do not install the package, simply check for and report potential
	conflicts.

# ERASE OPTIONS

The general form of an *rpm* erase command is

*rpm* {*-e*|*--erase*} [erase-options] _PACKAGE_NAME_ ...

The following erase-options options may also be used:

*--allmatches*
	Remove all versions of the package which match _PACKAGE_NAME_.
	Normally an error is issued if _PACKAGE_NAME_ matches multiple
	packages.

*--justdb*
	Update only the database, not the filesystem.

*--nodeps*
	Don't check dependencies before uninstalling the packages.

*--test*
	Don't really uninstall anything, just go through the motions.
	Useful in conjunction with the *-vv* option for debugging.

# QUERY OPTIONS

The general form of an *rpm* query command is

*rpm* {*-q*|*--query*} [select-options] [query-options] [_PACKAGE_FILE_ ...] [_PACKAGE_NAME_ ...]

You may specify the format that package information should be printed
in. To do this, you use the

*--qf*|*--queryformat* _QUERYFMT_

option, followed by the _QUERYFMT_ format string. Query formats are
modified versions of the standard *printf*(3) formatting. The format
is made up of static strings (which may include standard C character
escapes for newlines, tabs, and other special characters (not including 0))
and *printf*(3) type formatters. As *rpm* already knows the type to
print, the type specifier must be omitted however, and replaced by the
name of the header tag to be printed, enclosed by *{}* characters. Tag
names are case insensitive, and the leading *RPMTAG_* portion of the
tag name may be omitted as well.

Alternate output formats may be requested by following the tag with
*:typetag*. Currently, the following types are supported:

*:armor*
	Wrap a public key in ASCII armor.

*:arraysize*
	Display number of elements in array tags.

*:base64*
	Encode binary data using base64.

*:date*
	Use *strftime*(3) "%c" format.

*:day*
	Use *strftime*(3) "%a %b %d %Y" format.

*:depflags*
	Format dependency comparison operator.

*:deptype*
	Format dependency type.

*:expand*
	Perform macro expansion.

*:fflags*
	Format file flags.

*:fstate*
	Format file state.

*:fstatus*
	Format file verify status.

*:hex*
	Format in hexadecimal.

*:octal*
	Format in octal.

*:humaniec*
	Human readable number (in IEC 80000). The suffix K = 1024, M =
	1048576, ...

*:humansi*
	Human readable number (in SI). The suffix K = 1000, M = 1000000,
	...

*:json*
	Wrap data in JSON.

*:perms*
	Format file permissions.

*:pgpsig*
	Display signature fingerprint and time.

*:shescape*
	Escape single quotes for use in a script.

*:string*
	Display string format. (default)

*:tagname*
	Display tag name.

*:tagnum*
	Display tag number.

*:triggertype*
	Display trigger suffix.

*:vflags*
	File verification flags.

*:xml*
	Wrap data in simple xml markup.

For example, to print only the names of the packages queried, you could
use *%{NAME}* as the format string. To print the packages name and
distribution information in two columns, you could use
*%-30{NAME}%{DISTRIBUTION}*. *rpm* will print a list of all of the
tags it knows about when it is invoked with the *--querytags*
argument.

There are three subsets of options for querying: package selection, file
selection and information selection.

# PACKAGE SELECTION OPTIONS

_PACKAGE_NAME_

	Query installed package named _PACKAGE_NAME_. To specify the
	package more precisely the package name may be followed by the
	version or version and release both separated by a dash or an
	architecture name separated by a dot. See the output of *rpm -qa*
	or *rpm -qp* _PACKAGE_FILE_ as an example.

*-a*, *--all* [_SELECTOR_ ...]
	Query all installed packages.

	Optional _SELECTOR_'s in the form of tag=pattern can be provided to
	narrow the selection, for example name="b\*" to query packages whose
	name starts with "b".

*--dupes*
	List duplicated packages.

*-f*, *--file* _FILE_
	Query package owning installed _FILE_.

*--filecaps*
	List file names with POSIX1.e capabilities.

*--fileclass*
	List file names with their classes (libmagic classification).

*--filecolor*
	List file names with their colors (0 for noarch, 1 for 32bit, 2 for
	64 bit).

*--fileprovide*
	List file names with their provides.

*--filerequire*
	List file names with their requires.

*-g*, *--group* _GROUP_
	Query packages with the group of _GROUP_.

*--noglob*
	Do not glob arguments when installing package files.

*--nomanifest*
	Don't process non-package files as manifests.

*-p*, *--package* _PACKAGE_FILE_
	Query an (uninstalled) package _PACKAGE_FILE_. The _PACKAGE_FILE_
	may be specified as an *ftp* or *http* style URL, in which case
	the package header will be downloaded and queried. See *FTP/HTTP
	OPTIONS* for information on *rpm*'s *ftp* and *http* client
	support. The _PACKAGE_FILE_ argument(s), if not a binary package,
	will be interpreted as an ASCII package manifest unless
	*--nomanifest* option is used. In manifests, comments are
	permitted, starting with a '*#*', and each line of a package
	manifest file may include white space separated glob expressions,
	including URL's, that will be expanded to paths that are
	substituted in place of the package manifest as additional
	_PACKAGE_FILE_ arguments to the query.

*--path* _PATH_
	Query package(s) owning _PATH_, whether the file is installed or not.
	Multiple packages may own a _PATH_, but the file is only owned by the
	package installed last.

*--querybynumber* _HDRNUM_
	Query the _HDRNUM_ database entry directly; this is useful only
	for debugging.

*--specfile* _SPECFILE_
	Obsolete, use *rpmspec*(1) instead.

*--tid* _TID_
	Query package(s) that have a given _TID_ transaction identifier. A
	UNIX time stamp is currently used as a transaction identifier. All
	package(s) installed or erased within a single transaction have a
	common identifier.

*--triggeredby* _PACKAGE_NAME_
	Query packages that are triggered by package(s) _PACKAGE_NAME_.

*--whatobsoletes* _CAPABILITY_
	Query all packages that obsolete _CAPABILITY_ for proper
	functioning.

*--whatprovides* _CAPABILITY_
	Query all packages that provide the _CAPABILITY_ capability.

*--whatrequires* _CAPABILITY_
	Query all packages that require _CAPABILITY_ for proper functioning.

*--whatconflicts* _CAPABILITY_
	Query all packages that conflict with _CAPABILITY_.

*--whatrecommends* _CAPABILITY_
	Query all packages that recommend _CAPABILITY_.

*--whatsuggests* _CAPABILITY_
	Query all packages that suggest _CAPABILITY_.

*--whatsupplements* _CAPABILITY_
	Query all packages that supplement _CAPABILITY_.

*--whatenhances* _CAPABILITY_
	Query all packages that enhance _CAPABILITY_.

# PACKAGE QUERY OPTIONS

*--changelog*
	Display change information for the package.

*--changes*
	Display change information for the package with full time stamps.

*--conflicts*
	List capabilities this package conflicts with.

*--dump*
	Dump file information as follows (implies *-l*):

	path size mtime digest mode owner group isconfig isdoc rdev symlink

*--enhances*
	List capabilities enhanced by package(s).

*--filesbypkg*
	List all the files in each selected package.

*--filetriggers*
	List filetrigger scriptlets from package(s).

*-i*, *--info*
	Display package information, including name, version, and
	description. This uses the *--queryformat* if one was specified.

*--last*
	Orders the package listing by install time such that the latest
	packages are at the top.

*-l*, *--list*
	List files in package.

*--obsoletes*
	List packages this package obsoletes.

*--provides*
	List capabilities this package provides.

*--recommends*
	List capabilities recommended by package(s).

*-R*, *--requires*
	List capabilities on which this package depends.

*--suggests*
	List capabilities suggested by package(s).

*--supplements*
	List capabilities supplemented by package(s).

*--scripts*
	List the package specific scriptlet(s) that are used as part of the
	installation and uninstallation processes.

*-s*, *--state*
	Display the *states* of files in the package (implies *-l*). The
	state of each file is one of *normal*, *not installed*, or
	*replaced*.

*--triggers*, *--triggerscripts*
	Display the trigger scripts, if any, which are contained in the
	package.

*--xml*
	Format package headers as XML.

# FILE SELECTION OPTIONS

*-A*, *--artifactfiles*
	Only include artifact files (implies *-l*).

*-c*, *--configfiles*
	Only include configuration files (implies *-l*).

*-d*, *--docfiles*
	Only include documentation files (implies *-l*).

*-L*, *--licensefiles*
	Only include license files (implies *-l*).

*--noartifact*
	Exclude artifact files.

*--noconfig*
	Exclude config files.

*--noghost*
	Exclude ghost files.

# VERIFY OPTIONS

The general form of an *rpm* verify command is

*rpm* {*-V*|*--verify*} [select-options] [verify-options] [_PACKAGE_NAME_ ...]

Verifying a package compares information about the installed files in
the package with information about the files taken from the package
metadata stored in the rpm database. Among other things, verifying
compares the size, digest, permissions, type, owner and group of each
file. Any discrepancies are displayed. Files that were not installed
from the package, for example, documentation files excluded on
installation using the "*--excludedocs*" option, will be silently
ignored.

The package and file selection options are the same as for package
querying (including package manifest files as arguments). Other options
unique to verify mode are:

*--nodeps*
	Don't verify dependencies of packages.

*--nodigest*
	Don't verify package or header digests when reading.

*--nofiles*
	Don't verify any attributes of package files.

*--noscripts*
	Don't execute the *%verifyscript* scriptlet (if any).

*--nosignature*
	Don't verify package or header signatures when reading.

*--nolinkto*
*--nofiledigest* (formerly *--nomd5*)
*--nosize*
*--nomtime*
*--nomode*
*--nordev*
	Don't verify the corresponding file attribute.

*--nouser*
*--nogroup*
	Don't verify file user/group ownership. Note that only local
	*passwd*(5) and *group*(5) databases are consulted.

*--nocaps*
	Don't verify file capabilities.

The format of the output is a string of 9 characters, a possible
attribute marker:

[[ *Marker*
:- *%files attribute*
:< *Description*
|  *a*
:  %*a*rtifact
:  a build side-effect file (such as buildid links)
|  *c*
:  %*c*onfig
:  configuration file
|  *d*
:  %*d*oc
:  documentation file
|  *g*
:  %*g*host
:  ghost file (i.e. the file contents are not included in the package payload)
|  *l*
:  %*l*icense
:  license file
|  *m*
:  %*m*issingok
:  file missing is not a verify failure
|  *n*
:   %config(*n*oreplace)
:  do not replace config file
|  *r*
:   %*r*eadme
:  readme file
|  *s*
:  N/A
:  *s*pec file in source package

from the package header, followed by the file name. Each of the 9
characters denotes the result of a comparison of attribute(s) of the
file to the value of those attribute(s) recorded in the database. A
single "*.*" (period) means the test passed, while a single
"*?*" (question mark) indicates the test could not be performed
(e.g. file permissions prevent reading). Otherwise, the (mnemonically
em*B*oldened) character denotes failure of the corresponding
*--verify* test:

[[ *Character*
:< *Description*
|  *S*
:  file *S*ize differs
|  *M*
:  *M*ode differs (includes permissions and file type)
|  *5*
:  digest (formerly MD*5* sum) differs
|  *D*
:  *D*evice major/minor number mismatch
|  *L*
:  read*L*ink(2) path mismatch
|  *U*
:  *U*ser ownership differs
|  *G*
:  *G*roup ownership differs
|  *T*
:  m*T*ime differs
|  *P*
:  ca*P*abilities differ

# MISCELLANEOUS COMMANDS

*rpm* *--showrc*
	shows the values *rpm* will use for all of the options which are
	currently set in *rpmrc* and *macros* configuration file(s).

*rpm* *--setperms* | *--setugids* | *--setcaps* [_PACKAGE_NAME_ ...]
	obsolete aliases for *--restore*

*rpm* *--restore* [select-options] [_PACKAGE_NAME_ ...]
	The option restores file metadata such as timestamp, owner, group,
	permissions and capabilities of files in packages.

# FTP/HTTP OPTIONS

*rpm* can act as an FTP and/or HTTP client so that packages can be
queried or installed from the internet. Package files for install,
upgrade, and query operations may be specified as an *ftp* or *http*
style URL:

_http://HOST[:PORT]/path/to/package.rpm_

_ftp://[USER:PASSWORD]@HOST[:PORT]/path/to/package.rpm_

If both the user and password are omitted, anonymous *ftp* is used.

*rpm* allows the following options to be used with ftp URLs:

*rpm* allows the following options to be used with *http* and *ftp* URLs:

*--httpproxy* _HOST_
	The host _HOST_ will be used as a proxy server for all *http* and
	*ftp* transfers. This option may also be specified by configuring
	the macro *%\_httpproxy*.

*--httpport* _PORT_
	The TCP _PORT_ number to use for the *http* connection on the
	proxy http server instead of the default port. This option may also
	be specified by configuring the macro *%\_httpport*.

# FILES

## rpmrc Configuration

Each file in the colon separated rpmrc path is read sequentially by
*rpm* for configuration information. Only the first file in the
list must exist, and tildes will be expanded to the value of
*$HOME*. The default rpmrc path is as follows:

```
	/usr/lib/rpm/rpmrc:
	/usr/lib/rpm/<vendor>/rpmrc:
	/etc/rpmrc:
	~/.config/rpm/rpmrc
```

If *XDG_CONFIG_HOME* environment variable is set, it replaces _~/.config_
in the path.

In older rpm versions the path of per-user rpmrc was _~/.rpmrc_.
This is still processed if it exists and the new configuration directory
does not exist.

## Macro Configuration

Each file or *glob*(7) pattern in the colon-separated macro path is
read sequentially by *rpm* for macro definitions. Tildes will be expanded
to the value of the environment variable *HOME*. The default macro path
is as follows:

```
	/usr/lib/rpm/macros:
	/usr/lib/rpm/macros.d/macros.*:
	/usr/lib/rpm/platform/%{\_target}/macros:
	/usr/lib/rpm/fileattrs/*.attr:
	/usr/lib/rpm/<vendor>/macros:
	/etc/rpm/macros.*:
	/etc/rpm/macros:
	/etc/rpm/%{\_target}/macros:
	~/.config/rpm/macros
```

If *XDG_CONFIG_HOME* environment variable is set, it replaces _~/.config_
in the path.

In older versions of rpm, the path of per-user macros was _~/.rpmmacros_.
This is still processed if it exists and the new configuration directory
does not exist.

## Database

	/_var/lib/rpm/_

## Temporary

	_/var/tmp/rpm\*_

# SEE ALSO

*rpm-misc*(8), *popt*(3), *rpm2cpio*(1), *rpmbuild*(1), *rpmdb*(8),
*rpmkeys*(8), *rpmsign*(1), *rpmspec*(1)

*rpm --help* - as *rpm* supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
