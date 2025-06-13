RPM-RPMRC(5)

# NAME
*rpm-rpmrc* - rpm platform compatibility configuration

# SYNOPSIS
_VARIABLE_: {_ARCH_|_OS_}: _VALUE_ ...

_VARIABLE_: _ARCH_ _VALUE_

# FILES
_/usr/lib/rpm/rpmrc_++
_/usr/lib/rpm/<vendor>/rpmrc_++
_/etc/rpmrc_++
_~/.config/rpm/rpmrc_

# DESCRIPTION
The low-level machine architecture and OS configuration in *rpm* is
managed via a set of rpmrc files as defined by the _rpmrc path_.
Most users never need to look at, much less touch these files.

Each file in the colon separated _rpmrc path_ is read sequentially by
*rpm* for configuration information. Tildes will be expanded to the
value of the environment variable *HOME*.
The first file in the path must exist, others are considered optional.

If a value is defined multiple times, the last entry wins. The default
_rpmrc path_ uses this to achieve the following hierarchy of platform
configuration:
. Generic *rpm* factory defaults
. Vendor (distribution) specific configuration
. Host specific configuration
. User specific configuration

In older rpm versions the path of per-user rpmrc was _~/.rpmrc_.
This is still processed if it exists and the new configuration directory
does not exist.

# CONFIGURATION
_ARCH_ and _OS_ relate to *uname*(2) machine and operating system
information, but are not 1:1 equivalent.
The following directives are supported in the rpmrc files:

*arch_canon* _ARCH_: _CANON_ARCH_ _ARCH_NUM_
	Names and numbers of known architectures to
	alias different spellings to a canonical one.
	_CANON_ARCH_ is what the _ARCH_ entries in other rpmrc
	directives refer to.

	The number is not used for any calculations by *rpm* but
	must be present for historical reasons.

*arch_compat*: _ARCH_: _COMPAT_ARCH_ ...
	Declare compatibility between machine architectures,
	ie. _ARCH_ machines can install packages for _COMPAT_ARCH_
	architecture.

*archcolor*: _ARCH_ _COLOR_
	Declare the "color" of _ARCH_. The color relates to the word length
	aka bitness of the architecture:
	- *0* means none (noarch packages and similar)
	- *1* means 32-bit
	- *2* means 64-bit

*buildarch_compat*: _ARCH_: _COMPAT_ARCH_ ...
	Declare compatibility between build architectures targets,
	ie. _ARCH_ machines can produce _COMPAT_ARCH_ binaries.

*buildarchtranslate*: _ARCH_: _TRANSLATE_ARCH_
	Automatically translate detected host architecture _ARCH_ to
	_TRANSLATE_ARCH_ when building packages. This is used to map
	sub-architectures to a main one, for example when building on a
	*x86_64_v2* host we typically want the generated package to be
	of the main *x86_64* architecture.

*optflags*: _ARCH_ _OPTFLAGS_
	Compiler flags to use when building packages for _ARCH_ architecture.
	The _OPTFLAGS_ value is available as *%{optflags}* macro in
	spec files.

*os_canon*: _OS_: _CANON_OS_ _OS_NUM_
	Names and numbers of known operating systems to
	alias different spellings to a canonical one.
	_CANON_OS_ is what the _OS_ entries in other rpmrc directives
	refer to.

	The number is not used for any calculations by *rpm* but
	must be present for historical reasons.

*os_compat*: _OS_: _COMPAT_OS_ ...
	Declare compatibility between operating systems, ie.
	_OS_ machine can install packages for _COMPAT_OS_ operating
	system.

# ENVIRONMENT
If *XDG_CONFIG_HOME* environment variable is set, it replaces _~/.config_
in the _rpmrc path_.

# SEE ALSO
*rpm*(8), *rpm-common*(8), *rpm-config*(5), *rpm-macros*(7)
