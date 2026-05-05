RPM-SPEC-MACROS(7)

# NAME
rpm-spec-macros - RPM spec macros

# SYNOPSIS
%_NAME_

%_NAME_ [_OPTIONS_] [_ARGUMENTS_]

%_NAME_ [...]

# DESCRIPTION

This manual describes RPM macros that are specific to *rpm-spec*(5) files.
Distributions commonly ship with numerous additional build helper macros for
different language ecosystems and so on, documented here are only the
macros shipped with RPM itself.

For general RPM macro syntax and usage, see *rpm-macros*(7) and
*rpm-macrofile*(5).

## Build steps
*%autopatch* [_OPTIONS_] [_NUMBER_ ...]
	Apply (a range of) patch numbers. Use in combination with
	*%autosetup -N* for finer control over patch application.

	Only available in the *%prep* section.

	*-m* _MIN_
		Apply patches starting from _MIN_ range.
	*-M* _MAX_
		Apply patches up to _MAX_ range.
	*-p* _N_
		Patch prefix stripping level (same as *-p* to *patch*(1))
	*-q*
		Don't warn if there are no matching patches.
	*-v*
		Verbose operation.

*%autosetup* [_OPTIONS_]
	Prepare sources for building: unpack source archives and apply
	patches. See also *%setup*.

	Only available in the *%prep* section.

	*-a* _N_
		Unpack source _N_ after changing to the build directory.
		If multiple instances are needed, use *%setup* instead.
	*-b* _N_
		Unpack source _N_ before changing to the build directory.
		If multiple instances are needed, use *%setup* instead.
	*-c*
		Create the build directory (and change to it) before unpacking.
	*-C*
		Create the build directory and ensure the archive contents
		are unpacked there, stripping the top level directory in the
		archive paths if one exists.
	*-D*
		Do not delete the build directory prior to unpacking (used
		when more than one source is to be unpacked with *-a* or *-b*).
	*-n*
		Set the name of build directory
		(default is *%{name}-%{version}* macro-expanded).
	*-N*
		Disable automatic patch application (used for manual
		*%autopatch* invocation).
	*-p* _N_
		Patch prefix stripping level (same as *-p* to *patch*(1)
		and *%patch*).
	*-S* _VCS_
		Specify the Version Control System to use. Supported VCS's
		are:
		- *bzr*
		- *gendiff*
		- *git*
		- *git_am*
		- *hg*
		- *patch* (default)
		- *quilt*

		Note that the exact behavior depends on the VCS: for example
		*quilt*(1) only controls patches whereas *git*(1) and *hg*(1)
		control the entire source repository.
	*-T*
		Skip the default unpacking of the first source (mostly used
		with *-a* or *-b*).
	*-v*
		Verbose operation.

*%configure* [...]
	Invoke a _configure_ script of an *autoconf*(1) based software
	with appropriate options for RPM integration, such as preconfigured
	system paths. Any options and arguments are passed directly to the
	script.

	Typical use (but common also in *%build* section of older spec files):
	```
	%conf
	%configure --enable-wibble
	```

*%find_lang* [_OPTIONS_] _NAME_ [_PREFIX_]
	Look for translation files in the buildroot, generating a _NAME_*.lang*
	*%files* manifest for them.
	_NAME_ is usually the upstream project name.

	Available options:

	*--all-name*
		Match all package/domain names.
	*--generate-subpackages*
		Place translations in per-language sub-packages.
	*--with-gnome*
		Find GNOME help files.
	*--with-html*
		Find HTML files.
	*--with-kde*
		Find KDE help files.
	*--with-mate*
		Find MATE help files.
	*--with-qt*
		Find QT locale files.
	*--with-man*
		Find translated *man*(1) pages.
	*--without-mo*
		Do not find locale files.

	Typical use:
	```
	%install
	%make_install
	%find_lang %{name}

	%files -f %{name}.lang
	```

*%make_build* [...]
	Invoke *make*(1) for build purposes, with appropriate options for
	RPM integration, such as managing the number of parallel jobs.
	Any options and arguments are passed directly to *make*(1).

	Typical use:
	```
	%build
	%make_build
	```

*%make_install* [...]
	Invoke *make*(1) for install purposes, with appropriate options for
	RPM integration, such as *DESTDIR* pointed at *$RPM_BUILD_ROOT*.
	Any options and arguments are passed directly to *make*(1).

	Typical use:
	```
	%install
	%make_install
	```

*%patch* [_OPTIONS_] [_NUMBER_ ...]
	Apply patch numbers specified in arguments.
	See also *%autosetup* and *%autopatch*.

	Only available in the *%prep* section.

	*-b* _SUF_
		Backup patched files with suffix _SUF_.
	*-d* _DIR_
		Change to directory _DIR_ before doing anything else.
	*-E*
		Remove files emptied by patching.
	*-F* _N_
		Maximum fuzz factor (context diffs).
	*-p* _N_
		Patch prefix stripping level (same as *-p* to *patch*(1))
	*-R*
		Assume reversed patch.
	*-o* _FILE_
		Send output to FILE instead of patching in place.
	*-z* _SUF_
		Same as *-b*.
	*-Z*
		Set file _mtime_ and _atime_ from patch headers using UTC
		(context diffs).
	*-P* _N_
		Apply patch number _N_.
		Same as passing _N_ as a positional argument.

*%setup* [_OPTIONS_]
	Unpack sources. See also *%autosetup*.

	Only available inside the *%prep* section.

	*-a* _N_
		Unpack source _N_ after changing to the build directory.
	*-b* _N_
		Unpack source _N_ before changing to the build directory.
	*-c*
		Create the build directory (and change to it) before unpacking.
	*-C*
		Create the build directory and ensure the archive contents
		are unpacked there, stripping the top level directory in the
		archive paths if one exists.
	*-D*
		Do not delete the build directory prior to unpacking (used
		when more than one source is to be unpacked with *-a* or *-b*).
	*-n*
		Set the name of build directory
		(default is *%{name}-%{version}* macro-expanded).
	*-T*
		Skip the default unpacking of the first source (mostly used
		with *-a* or *-b*).
	*-q*
		Quiet operation.

## Sources and patches
*%{P:*_NUMBER_*}*
	Shorthand for *%{PATCH*​_NUMBER_*}*.

*%PATCH*​_NUMBER_
	Expand to the filename of the specified Patch _NUMBER_.

*%PATCHURL*​_NUMBER_
	Expand to the full URL of the specified Patch _NUMBER_.

*%patches*
	Space separated list of all the patches in the package.

*%{S:*_NUMBER_*}*
	Shorthand for *%{SOURCE*​_NUMBER_*}*.

*%SOURCE*​_NUMBER_
	Expand to the filename of the specified *Source* _NUMBER_.

*%SOURCEURL*​_NUMBER_
	Expand to the full URL of the specified *Source* _NUMBER_.

*%sources*
	Space separated list of all the sources in the package.

## Conditional builds

*%bcond* _NAME_ _DEFAULT_
	Create a build conditional for a feature called _NAME_, defaulting to
	either on (*1*) or off (*0*).

	Added: 4.17.1

	```
	# Create a "gnutls" build conditional, enabled by default:
	%bcond gnutls 1
	# Create a "bootstrap" build conditional, disabled by default:
	%bcond bootstrap 0
	```

*%bcond_with* _NAME_
	Deprecated. Identical to *%bcond* _NAME_ *0*.

*%bcond_without* _NAME_
	Deprecated. Identical to *%bcond* _NAME_ *1*.

*%{with* _NAME_*}*
	Test if build conditional _NAME_ is enabled.

	```
	%if %{with gnutls}
	BuildRequires: gnutls-devel
	%endif
	```

*%{without* _NAME_*}*
	Test if build conditional _NAME_ is disabled.

*%with_*​_NAME_
	Alternative form of testing a build conditional.

	```
	%configure \\
		%{?with_static:--enable-static} \\
		%{!?with_static:--disable-static}
	```

See _http://rpm.org/docs/latest/manual/conditionalbuilds.html_ for more
information.

## Path macros
RPM defines paths for various common locations in UNIX-like systems for
the sake of flexibility and some portability.
Most of these macros honor the prefix defined in *%\_prefix*
(not to be confused with *%prefix* from relocatable packages).

The values in parenthesis reflect common values of these macros on modern
Linux systems and are provided for mental reference only.

*%\_bindir*
	Most user commands. (_/usr/bin_)
*%\_datadir*
	Architecture independent data. (_/usr/share_)
*%\_datarootdir*
	Alias for *%\_datadir*.
*%\_includedir*
	Standard include files. (_/usr/include_)
*%\_infodir*
	Info pages. (_/usr/share/info_)
*%\_libdir*
	Standard libraries. (_/usr/lib_ or _/usr/lib64_)
*%\_libexecdir*
	Binaries run by other commands. (_/usr/libexec_)
*%\_localstatedir*
	Variable data files. (_/var_)
*%\_mandir*
	Manual pages. (_/usr/share/man_)
*%\_prefix*
	Prefix of most path macros. (_/usr_)
*%\_rundir*
	Run-time variable data. (_/run_)
*%\_sharedstatedir*
	Variable state information. (_/var/lib_)
*%\_sbindir*
	Non-essential standard system binaries. (_/usr/sbin_)
*%\_sysconfdir*
	Host-specific system configuration. (_/etc_)

For historical reasons, the path name macros follow *autoconf*(1) practises.

## Architecture families
Macros for testing architecture families with *%ifarch* conditionals.

Typical use:
```
%ifarch %{arm32}
BuildRequires: arm-and-leg-devel
%endif
```

*%alpha*
	Alpha processors.
*%arm32*
	32-bit ARM processors.
*%arm64*
	64-bit ARM processors.
*%e2k*
	E2K processors.
*%ix86*
	32-bit x86 processors.
*%loongarch64*
	64-bit LOONGARCH processors.
*%mips*
	All MIPS processors.
*%mips32*
	32-bit MIPS processors.
*%mips64*
	64-bit MIPS processors.
*%mipseb*
	Big-endian MIPS processors.
*%mipsel*
	Little-endian MIPS processors.
*%power64*
	64-bit PowerPC processors.
*%riscv32*
	32-bit RISC-V processors.
*%riscv64*
	64-bit RISC-V processors.
*%riscv128*
	128-bit RISC-V processors.
*%riscv*
	All RISC-V processors.
*%sparc*
	All Sparc processors.
*%x86_64*
	64-bit x86 processors.

## Compiler flags

*%build_cflags*
	C compiler flags.  This is traditionally called *CFLAGS* in makefiles.

*%build_cxxflags*
	C++ compiler flags.  This is traditionally called *CXXFLAGS* in makefiles.

*%build_fflags*
	Fortran compiler flags.  Makefiles use both *FFLAGS* and *FCFLAGS* as
	the corresponding variable names.

*%optflags*
	Unspecified compiler flags from *rpm-rpmrc*(5).

	Deprecated, use one of the language specific *%build_\*flags* macros
	instead.

*%set_build_flags*
	Shell code to set various compiler/linker environment
	variables such as *CFLAGS* and *LDFLAGS* if they have not been
	set already.

## Platform macros
*%\_host*
	Shorthand for *%{\_host\_cpu}-%{\_host\_os}*

*%\_host\_cpu*
	The architecture of the host where RPM is running.

*%\_host\_os*
	The operating system of the host where RPM is running.

*%\_target*
	Shorthand for *%{\_target\_cpu}-%{\_target\_os}*.

*%\_target\_cpu*
	The architecture for which the current package is being built for.

*%\_target\_os*
	The operating system for which the current package is being built for.

# SEE ALSO
*rpmbuild*(1) *rpm-spec*(5) *rpm-macros*(7)
