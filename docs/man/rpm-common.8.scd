RPM-COMMON(8)

# NAME
rpm-common - Operations and options common to the main *rpm*(8) executables

# SYNOPSIS
_RPMCMD_ [options] {*-?*|*--help*}

_RPMCMD_ [options] *--showrc*

_RPMCMD_ [options] *--version*

# DESCRIPTION
The following _RPMCMD_ executables share common configuration and the
options and operations documented in this manual:
- *rpm*(8)
- *rpmdb*(8)
- *rpmkeys*(8)
- *rpmbuild*(1)
- *rpmsign*(1)
- *rpmspec*(1)

# OPERATIONS
*-?*,
*--help*
	Print a longer usage message than normal.

*--version*
	Print a single line containing the version number of *rpm* being
	used.

*--showrc*
	Dump information about *rpm* configuration to standard output.

# OPTIONS
*--color* <_MODE_>
	Use terminal colors for highlighting error and debug message,
	where _MODE_ is one of:
	- *always*
	- *auto* (use color output on terminals)
	- *never* (default)

*--dbpath* _DIRECTORY_
	Use the database in _DIRECTORY_ rather than the default path
	_/var/lib/rpm_.

*-D* '_MACRO_ _EXPR_',
*--define*='_MACRO_ _EXPR_'
	Defines _MACRO_ with value _EXPR_.

*-E* '_EXPR_',
*--eval*='_EXPR_'
	Prints *rpm-macros*(7) expansion of _EXPR_.

*--load* _FILE_
	Load an individual *rpm-macrofile*(5).

*--macros* _FILELIST_
	Replace the list of macro files to be loaded with _FILELIST_.
	See *Macro Configuration* for details.

*--nodigest*
	Don't verify package or header digests when reading.

*--nohdrchk*
	Don't verify database header(s) when retrieved.

*--nosignature*
	Don't verify package or header signatures when reading.

*--pipe* _CMD_
	Pipes the output of *rpm* to the command _CMD_.

*--predefine*='_MACRO_ _EXPR_'
	Defines _MACRO_ with value _EXPR_ before loading macro files.
	This is a very rare special-purpose switch, use *--define*
	for the common macro definition needs.

*--target* _PLATFORM_
	Use _PLATFORM_ configuration instead of detecting automatically.
	_PLATFORM_ is formed as _arch_[-_os_].

*--quiet*
	Print as little as possible - normally only error messages will be
	displayed.

*--rcfile* _FILELIST_
	Replace the default list of configuration files to be read with
	_FILELIST_.  See *rpmrc Configuration* for details.

*--root* _DIRECTORY_
	Use the filesystem tree rooted at _DIRECTORY_ for all operations.
	Note that this means the database within _DIRECTORY_ will be used
	for dependency checks and any scriptlet(s) (e.g. *%post* if
	installing, or *%prep* if building, a package) will be run after a
	*chroot*(2) to _DIRECTORY_.

	Note that rpm assumes the environment inside the root is set up by
	the caller, such as any mounts needed for the operation inside the
	root directory.

*--undefine*='_MACRO_'
	Undefines _MACRO_.

*-v*,
*--verbose*
	Print verbose information - normally routine progress messages will
	be displayed. Supply more than once to increase verbosity, eg. *-vv*.

# DEBUG OPTIONS
*--debug*
	Print lots of debug information.

*--fsmdebug*
	Print debug information about payload handling.

*--rpmiodebug*
	Print debug information about file IO.

*--stats*
	Print runtime statistics of often used functions.

# FTP/HTTP OPTIONS
These options are deprecated. Use special purpose software such as
*curl*(1) or *wget*(1) for fetching data from the internet.

For some operations, *rpm* can act as an FTP and/or HTTP client so that
packages can be queried or installed from the internet. Package files for
install, upgrade, and query operations may be specified as an *ftp* or *http*
style URL:

_http://HOST[:PORT]/path/to/package.rpm_

_ftp://[USER:PASSWORD]@HOST[:PORT]/path/to/package.rpm_

If both the user and password are omitted, anonymous *ftp* is used.

*rpm* allows the following options to be used with *http* and *ftp* URLs:

*--httpproxy* _HOST_
	Use _HOST_ as the proxy server for all *http* and
	*ftp* transfers. This option may also be specified by configuring
	the macro *%\_httpproxy*.

*--httpport* _PORT_
	The TCP _PORT_ number to use for the *http* connection on the
	proxy http server instead of the default port. This option may also
	be specified by configuring the macro *%\_httpport*.

# ENVIRONMENT
*RPM_CONFIGDIR*
	Used to override the default *rpm* configuration home,
	typically _/usr/lib/rpm_.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# BUGS
Not all options are meaningful or implemented in all the executables
they show in. In particular, *--root* and the various verify-related
options are only honored by *rpm*(8) and *rpmdb*(8).

# FILES

## Database

```
/var/lib/rpm/
```

## Temporary

```
/var/tmp/rpm\*
```

# SEE ALSO
*popt*(3), *rpm*(8) *rpm-config*(5), *rpm-rpmrc*(5), *rpm-macros*(7)
