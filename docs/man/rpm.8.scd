RPM(8)

# NAME
rpm - RPM Package Manager

# SYNOPSIS
## Querying and verifying packages
*rpm* {*-q*|*--query*} [select-options] [query-options] [_PACKAGE_FILE_ ...] [_PACKAGE_NAME_ ...]

*rpm* {*-V*|*--verify*} [select-options] [verify-options] [_PACKAGE_NAME_ ...]

## Installing, upgrading and removing packages
*rpm* {*-i*|*--install*} [transaction-options] [install-options] _PACKAGE_FILE_ ...

*rpm* {*-U*|*--upgrade*} [transaction-options] [install-options] _PACKAGE_FILE_ ...

*rpm* {*-F*|*--freshen*} [transaction-options] [install-options] _PACKAGE_FILE_ ...

*rpm* *--reinstall* [transaction-options] [install-options] _PACKAGE_FILE_ ...

*rpm* *--restore* [transaction-options] [select-options] _PACKAGE_NAME_ ...

*rpm* {*-e*|*--erase*} [transaction-options] [erase-options] _PACKAGE_NAME_ ...

## Misc operations
*rpm* *--querytags*

See *rpm-common*(8) for operations common to all rpm executables.

# DESCRIPTION
*rpm* is a powerful *Package Manager*, which can be used to build,
install, query, verify, update, and erase individual software packages.
A *package* consists of an archive of files and meta-data used to
install and erase the archive files. The meta-data includes helper
scripts, file attributes, and descriptive information about the package.
*Packages* come in two varieties: binary packages, used to encapsulate
software to be installed, and source packages, containing the source
code and recipe necessary to produce binary packages.

# OPERATIONS
## Querying and verifying packages
*-V*,
*--verify*
	Verify package(s), comparing information about the installed files in
	the package with information about the files taken from the package
	metadata stored in the *rpm* database. Among other things, verifying
	compares the size, digest, permissions, type, owner and group of each
	file. Any discrepancies are displayed.
*-q*,
*--query*
	Query package files or installed package(s).

## Installing, upgrading and removing packages
*-i*,
*--install*
	Install new package(s) without considering upgrade scenarios.
	This is considered special usage, normally installations should
	be done with *--upgrade*.

*-U*,
*--upgrade*
	Install or upgrade package(s) to a newer version. Old and/or
	obsoleted package(s) are removed after the new package is installed.

*-F*,
*--freshen*
	Upgrade package(s), but only ones for which an earlier version
	is installed.

*--reinstall*
	Reinstall previously installed package(s).

*--restore*
	Restore file metadata such as timestamp, owner, group,
	permissions and capabilities of files of installed package(s).

*-e*,
*--erase*
	Erase installed packages.

## Misc operations
*--querytags*
	Dump known querytags. Useful with the *--queryformat* option.

See *rpm-common*(8) for the operations common to all rpm executables.

# ARGUMENTS
_PACKAGE_FILE_
	Either an *rpm* package or an *rpm-manifest*(5) file. May also be
	specified as an *ftp* or *http* URL, in which case the package will
	be downloaded before being installed. See *FTP/HTTP OPTIONS* for
	information on *rpm*'s *ftp* and *http* client support.

_PACKAGE_NAME_
	Installed package named _PACKAGE_NAME_. To specify the
	package more precisely the package name may be followed by the
	version or version and release both separated by a dash or an
	architecture name separated by a dot. See the output of *rpm -qa*
	or *rpm -qp* _PACKAGE_FILE_ as an example.

# OPTIONS
See *rpm-common*(8) for the options common to all operations.

# INSTALL AND UPGRADE OPTIONS
*--allfiles*
	Installs or upgrades all the *missingok* files in the package,
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

*--excludeconfigs*, *--noconfigs*
	Do not install configuration files.

*--excludedocs*
	Don't install any files which are marked as documentation (which
	includes man pages and texinfo documents).

*--force*
	Same as using *--replacepkgs*, *--replacefiles*, and
	*--oldpackage*.

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

*--nocontexts*
	Don't set SELinux contexts for files and scriptlets.
	Only effective if *rpm-plugin-selinux*(8) is installed
	and active.

*--nocaps*
	Don't set file capabilities.

*--noverify*
	Don't perform verify package files prior to installation.

*--nosysusers*
	Don't create sysusers from packages

*--oldpackage*
	Allow an upgrade to replace a newer package with an older one.

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

# ERASE OPTIONS
*--allmatches*
	Remove all versions of the package which match _PACKAGE_NAME_.
	Normally an error is issued if _PACKAGE_NAME_ matches multiple
	packages.

# TRANSACTION OPTIONS
The transaction options are common to *--install*, *--upgrade*,
*--reinstall* and *--erase* operations.

*--deploops*
	Print dependency loops as warnings.

*-h*, *--hash*
	Print 50 hash marks as the package archive is unpacked. Use with
	*-v*|*--verbose* for a nicer display.

*--justdb*
	Update only the database, not the filesystem.

*--nodb*
	Update only the filesystem, not the database.

*--nodeps*
	Don't check dependencies before uninstalling the packages.

*--noorder*
	Don't reorder the packages for an install. The list of packages
	would normally be reordered to satisfy dependencies.

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

*--percent*
	Print percentages as files are unpacked from the package archive.
	This is intended to make *rpm* easy to run from other tools.

*--test*
	Don't really execute anything, just go through the motions.
	Useful in conjunction with the *-vv* option for debugging.

# QUERY OPTIONS
*--qf* _QUERYFMT_,
*--queryformat* _QUERYFMT_
	Output format of each queried package, as described by
	*rpm-queryformat*(7). If omitted, uses the format defined by
	*%\_query\_all\_fmt* in *rpm-config*(5).

There are three subsets of options for querying: package selection, file
selection and information selection.

# PACKAGE SELECTION OPTIONS
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
	Don't process non-package files as *rpm-manifest*(5) files.

*-p*, *--package* _PACKAGE_FILE_
	Query an (uninstalled) package _PACKAGE_FILE_.

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

The package and file selection options are the same as for package
querying (including *rpm-manifest*(5) files as arguments).
Other options unique to verify mode are:

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

# OUTPUT
*--verify*
	The format of the output is a string of 9 _result_ characters
	a possible _attribute_ from the package, followed by the file name.

	Each of the 9 characters denotes the result of a comparison of
	attribute(s) of the file to the value of those attribute(s)
	recorded in the database.

	A single "*.*" (period) means the test passed, while a single
	"*?*" (question mark) indicates the test could not be performed
	(e.g. file permissions prevent reading). Otherwise, the (mnemonically
	em*B*oldened) character denotes failure of the corresponding
	*--verify* test:

|[ *Result*
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

|[ *Attribute*
:< *Description*
|  *a*
:  *%artifact* - an implicit side-effect file (eg. build-id links)
|  *c*
:  *%config*uration file
|  *d*
:  *%doc*umentation file
|  *g*
:  *%ghost* file
|  *l*
:  *%license* file
|  *m*
:  *%missingok* - file missing is not a verify failure
|  *n*
:  %config(*noreplace*) - do not replace (a %config file)
|  *r*
:  *%readme* file
|  *s*
:  *rpm-spec*(5) file in a source package


# EXIT STATUS
On success, 0 is returned, a non-zero failure code otherwise.

# FILES
See *rpm-common*(8), *rpm-config*(5) and *rpm-rpmrc*(5).

# SEE ALSO
*rpm-common*(8), *popt*(3), *rpm2cpio*(1), *rpmbuild*(1), *rpmdb*(8),
*rpmkeys*(8), *rpmsign*(1), *rpmspec*(1), *rpm-queryformat*(7)
*rpm-manifest*(5)

*rpm --help* - as *rpm* supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
