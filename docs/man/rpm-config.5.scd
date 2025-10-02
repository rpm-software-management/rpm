RPM-CONFIG(5)

# NAME
*rpm-config* - rpm runtime macro configuration

# SYNOPSIS
_NAME_ _VALUE_

# FILES
_/usr/lib/rpm/macros_++
_/usr/lib/rpm/macros.d/macros.\*_++
_/usr/lib/rpm/platform/%{\_target}/macros_++
_/usr/lib/rpm/fileattrs/\*.attr_++
_/usr/lib/rpm/<vendor>/macros_++
_/etc/rpm/macros.\*_++
_/etc/rpm/macros_++
_/etc/rpm/%{\_target}/macros_++
_~/.config/rpm/macros_

# DESCRIPTION
The primary configuration mechanism in *rpm* is *rpm-macros*(7).
On startup, *rpm* reads a set of *rpm-macrofile*(5) files determined
by the _macro path_.

Each file or *glob*(7) pattern in the colon-separated _macro path_ is
read sequentially by *rpm* for macro definitions. *%{\_target}* is
expanded to the detected <arch>-<os> platform. Tildes are expanded
to the value of the environment variable *HOME*.

If a macro is defined multiple times, the last entry wins. The default
_macro path_ uses this to achieve the following hierarchy of settings:
. Generic *rpm* factory defaults
. Platform-specific *rpm* factory defaults
. Vendor (distribution) specific settings
. Host specific settings
. User specific settings
. Command-line override settings

The default _macro path_ can be inspected with *rpm --showrc|grep ^Macro*.

In older versions of rpm, the path of per-user macros was _~/.rpmmacros_.
This is still processed if it exists and the new configuration directory
does not exist.

# CONFIGURATION
The following configurables are supported for the *rpm* runtime (as
opposed to just package building) parts:

*%\_color_output* _MODE_
	Output coloring mode. Valid values are *never* and *auto*.

*%\_db_backend* _BACKEND_
	The database backend to use. Possible values for _BACKEND_ are:
	- *dummy*: Dummy backend (no actual functionality)
	- *bdb_ro*: Berkeley DB (read-only)
	- *ndb*: Native database (no external dependencies)
	- *sqlite*: Sqlite database

*%\_dbpath* _DIRECTORY_
	The location of the rpm database file(s).

*%\_excludedocs* _VALUE_
	Boolean (i.e. 1 == "yes", 0 == "no") that controls whether files
	marked as %doc should be installed.

*%\_flush_io* _VALUE_
	Flush file IO during transactions (at a severe cost in performance
	for rotational disks). Possible values are 1 to enable, 0 to disable.

*%\_group_path* _PATH_
	Location of group(5) files as : separated list

*%\_httpport* _PORT_
	The port of HTTP proxy (used for FTP/HTTP).

*%\_httpproxy* _HOSTNAME_
	The hostname of HTTP proxy (used for FTP/HTTP).

*%\_install_langs* _LOCALES_
	A colon separated list of desired locales to be installed;
	*all* means install all locale specific files.

*%\_install_script_path* _PATH_
	The PATH used in *rpm-scriptlet*(7) execution environment.

*%\_keyring* _BACKEND_
	The keyring type to use. Possible values for _BACKEND_ are:
	- *fs*: Plain ASCII files in a directory
	- *openpgp*: Shared OpenPGP certificate directory
	- *rpmdb*: Pseudo-packages in the rpmdb

*%\_keyringpath* _DIRECTORY_
	The location of the keyring path for non-rpmdb variants.

*%\_minimize_writes* _VALUE_
	Minimize writes during transactions (at the cost of more reads) to
	conserve eg SSD disks (EXPERIMENTAL). Possible values are:
	- *0*: disable
	- *1*: enable
	- *-1*: (or undefined) autodetect on platforms where supported,
	  otherwise default to disabled

*%\_netsharedpath* _PATH_
	A colon separated list of paths where files should *not* be installed.
	Usually, these are network filesystem mount points.

*%\_passwd_path* _PATH_
	Location of passwd(5) files as : separated list

*%\_pkgverify_digests* _HASHALGOS_
	A colon separated list of hash algorithms to calculate digests on the
	entire package files during verification. The calculated digests
	are stored in the *Packagedigests* tag of packages in the rpmdb,
	and the corresponding algorithms in the *Packagedigestalgos* tag.
	No package digests are calculated or stored if *--noverify* is
	used during package installation.

*%\_pkgverify_flags* _VSFLAGS_
	Transaction package verification flags, used for fine-grained
	control of *%\_pkgverify_level* operation.
	Set to 0x0 for full compatibility with v4 packages.

*%\_pkgverify_level* _MODE_
	Enforced package verification mode in transactions,
	where _MODE_ is one of:
	- *all*: require valid digest(s) and signature(s)
	- *signature*: require valid signature(s)
	- *digest*: require valid digest(s)
	- *none*: legacy rpm behavior, nothing required

*%\_prefer_color* _VALUE_
	Package conflict resolution in bi-arch transactions.
	See also *%\_transaction_color*. Possible values are:
	- *0*: disabled
	- *1*: prefer 32-bit packages
	- *2*: prefer 64-bit packages

*%\_\_plugindir* _DIRECTORY_
	Transaction plugin directory.

*%\_query_all_fmt* _FORMAT_
	Default output format for *rpm*(8) query operations, as described by
	*rpm-queryformat*(7). Percent signs need to be escaped, for example
	*%%{nevra}*.

*%\_rpmlock_path* _FILE_
	The path of the file used for transaction fcntl lock.

*%\_tmppath* _PATH_
	The directory where temporary files are created.

*%\_\_urlhelpercmd* _EXECUTABLE_
	The executable to use for retrieving remote files.

*%\_\_urlhelperopts* _OPTIONS_
	Generic options to pass to the *%\_\_urlhelpercmd* command.

*%\_\_urlhelper_localopts* _OPTIONS_
	User/host specific options to pass to the *%\_\_urlhelpercmd* command.

*%\_\_urlhelper_proxyopts* _OPTIONS_
	Proxy options to pass to the *%\_\_urlhelpercmd* command.

*%\_urlhelper* _COMMAND_
	Full command (with options) to use when retrieving remote files.
	Normally pieced together from the double-underscore *%\_\_urlhelper\**
	macros.

*%\_transaction_color* _VALUE_
	Package and file conflict behavior in bi-arch transactions.
	See also *%\_prefer_color*. Possible values are:
	- *0*: do not consider "colors", only use arch compatibility map
	- *1*: only allow 32-bit packages
	- *2*: only allow 64-bit packages
	- *3*: allow 32- and 64-bit packages to share files

*%\_vsflags_erase* _VSFLAGS_
	Transaction verification flags used when erasing or updating packages.

*%\_vsflags_install* _VSFLAGS_
	Transaction verification flags used when installing packages.

*%\_vsflags_query* _VSFLAGS_
	Transaction verification flags used when querying packages.

*%\_vsflags_rebuilddb* _VSFLAGS_
	Transaction verification flags used when rebuilding the database.

*%\_vsflags_verify* _VSFLAGS_
	Transaction verification flags used when verifying packages.

## Verification flags
Digest/signature verification flags for various rpm operations are controlled
by a bitmask known as _VSFLAGS_. These flags control various aspects
of digital checksum and signature verification when reading rpm package
files and their headers.

_VSFLAGS_ is formed by bitwise or'ing the individual flags:
- *0x00001* (RPMVSF_NOHDRCHK): don't verify headers from rpmdb
- *0x00100* (RPMVSF_NOSHA1HEADER): don't verify header SHA1 digest
- *0x00200* (RPMVSF_NOSHA256HEADER): don't verify header SHA256 digest
- *0x00400* (RPMVSF_NODSAHEADER): don't verify header DSA signature(s)
- *0x00800* (RPMVSF_NORSAHEADER): don't verify header RSA signature(s)
- *0x01000* (RPMVSF_NOOPENPGP): don't verify header OpenPGP signature(s)
- *0x02000* (RPMVSF_NOSHA3_256HEADER): don't verify header SHA3_256 digest
- *0x10000* (RPMVSF_NOSHA256PAYLOAD): don't verify package payload SHA256 digest
- *0x20000* (RPMVSF_NOMD5): don't verify legacy header+payload MD5 digest
- *0x40000* (RPMVSF_NODSA): don't verify legacy header+payload DSA signature
- *0x80000* (RPMVSF_NORSA): don't verify legacy header+payload RSA signature

RPM's Python bindings can be helpful for working with these values,
for example:
```
>>> import rpm
>>> hex(rpm.RPMVSF_NOSHA1HEADER)
'0x100'
>>> hex(rpm.RPMVSF_NOSHA1HEADER|rpm.RPMVSF_NOMD5)
'0x20100'
>>>
```

## Hash algorithms
The following hash algorithms (denoted by _HASHALGO_) are known to rpm,
but availability can vary depending how rpm and it's underlying libraries
have been built:
- *1*: MD5 (obsolete)
- *2*: SHA1 (obsolete)
- *8*: SHA256
- *9*: SHA384
- *10*: SHA512
- *12*: SHA3-256
- *14*: SHA3-512

# ENVIRONMENT
If *XDG_CONFIG_HOME* environment variable is set, it replaces _~/.config_
in the _macro path_.

# SEE ALSO
*rpm*(8), *rpm-common*(8), *rpm-macrofile*(5), *rpm-rpmrc*(5), *rpm-macros*(7)
