RPM-SPEC(5)

# NAME
*rpm-spec* - RPM spec file format

# SYNOPSIS
## Preamble
_TAG_*:* _VALUE_++
_TAG_*:* _VALUE_ ...++
_TAG_[*(*_MODIFIERS_*)*]*:* _VALUE_ ...++
_TAG_[*(*_LANG_*)*]*:* _VALUE_

## Sections
*%*_SECTION_++
​...++
\[*%end*]

*%*_SECTION_ [*-n* _SUBNAME_] [options]++
​...++
\[*%end*]

; "." at the start of a line is special to scdoc, work around with unicode hack
*%*_SECTION_ [{*-a*|*-p*}]++
​...++
\[*%end*]

## Comments
*#* _LINE_ (preamble and most manifest sections, macros expanded)

*%dnl* _LINE_ (everywhere)

# DESCRIPTION
An RPM _spec_ file is a plain-text recipe for building one or more software
packages in the RPM format, in an automated, non-interactive and repeateable
manner, by using *rpmbuild*(1). See also *rpm-design*(7) for some background.

Each spec defines at least one package, known as the _main package_,
whose name and versioning information both the source and binary packages
share. The source package contains all the spec file itself, and all
the sources and patches declared in it. Binary packages are
produced by building the source package, as instructed by the spec.
The binary content is commonly split into further _sub-packages_ to make
packages easier to consume both size and dependency-wise.

The spec consists of multiple different, named blocks. The first block is
called the *PREAMBLE*, followed by one or more *SECTIONS*.
Sections start with a _directive_ line that consists of the section name
prefixed with the percent sign (*%*), followed by optional arguments.
Each block implicitly ends when another one starts, or when explicitly
terminated with the *%end* directive.

The spec file is parsed linearly from top to bottom. In most blocks, empty
and whitespace-only lines are ignored and lines starting with *#* are
considered comments. Except lines in the false branch of *CONDITIONALS*,
every spec line is macro-expanded before further processing, including
comment lines.

# PREAMBLE
The preamble consists of tag:value pairs, one tag per line.
Tags accepting multiple values on one line can also appear multiple
times within a package.

A *Properties* line documents various characteristics of the tag or section:
- *D*: Defines macro by the same name (uppercase on first encounter,
  lowercase for all except *Patch* and *Source*)
- *I*: Informative, only used for displaying
- *M*: Macro settable via *rpmbuild-config*(5)
- *O*: Obsolete, do not use
- *R*: Required tag/section
- *X*: Exclusive to *PREAMBLE*
- *b*: Stored in the binary packages
- *s*: Stored in the source package
- *t*: Build-time transient, not stored in packages

An example is shown in an indented block where applicable.

*AutoReq:* _BOOLEAN_
	Control per-package automatic dependency generation for *Requires*
	dependencies. Defaults to on.

	Usually it's better to disable/filter specific problematic
	dependencies than everything. See *rpm-dependency-generators*(7).

	Properties: t

	```
	AutoReq: 0
	```

*AutoProv:* _BOOLEAN_
	Control per-package automatic dependency generation for *Provides*
	capabilities. Default to on.

	Usually it's better to disable/filter specific problematic
	dependencies than everything. See *rpm-dependency-generators*(7).

	Properties: t

	```
	AutoProv: 0
	```

*AutoReqProv:* _BOOLEAN_
	*AutoReqProv* is equal to specifying both *AutoReq* and *AutoProv*
	to the same value.

	Properties: t

	```
	AutoReqProv: no
	```

*BugURL*: _URL_
	Bug reporting URL for the package.
	Usually set via *rpmbuild-config*(5).

	Properties: DIM

	```
	BugURL: https://bugzilla.mydistro.org
	```

*BuildArch*: _ARCH_ ...
	Architecture of this package, known to RPM through *rpm-rpmrc*(5)
	configuration. Normally the built package architecture is
	determined from the running system or *--target* option to
	*rpmbuild*(1), but *BuildArch* allows locking it to a predetermined
	value. The most common use is *noarch* for architecture independent
	content.

	As a special case, *BuildArch: noarch* can also be used in a
	sub-package to indicate architecture independent content such
	as documentation in an otherwise architecture specific package.

	For historical reasons, this field accepts multiple values but
	only the first one is used.

	Properties: bs

	```
	BuildArch: noarch
	```

*BuildConflicts:* _CAPABILITY_ ...
	Capabilities which cannot be installed during the
	package build.
	Resolved before building the package.

	Properties: s

	```
	BuildConflicts: somelib-devel
	```

*BuildPreReq:* _CAPABILITY_ ...
	Obsolete. Use *BuildRequires* instead.

	Properties: Os

*BuildRequires:* _CAPABILITY_ ...
	Capabilities required to build the package.
	Resolved before building the package.

	Properties: s

	```
	BuildRequires: e2fsprogs-devel >= 1.17-1
	```

*BuildRoot:* _PATH_
	Obsolete. Silently ignored in RPM >= 4.6.0.

	Properties: OPt

*BuildOption*[*(*_BUILDSCRIPT_*)*]: _OPTION_
	Declarative buildsystem option for _BUILDSCRIPT_.
	See *Build scriptlets* for valid names, used without the
	preceding *%* here.

	Properties: t

	```
	BuildOption(conf): --enable-fu
	```

*BuildSystem:* _NAME_
	Use declarative buildsystem _NAME_ to build the package.
	Buildsystems are inherently distribution specific.

	Properties: DXt

	```
	Buildsystem: cmake
	```

*Conflicts:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

	```
	Conflicts: another-tool <= 4.0
	```

*Distribution:* _TEXT_
	Distribution name.

	Properties: DIMbs

*DistTag:* _TEXT_
	Short label for identifying a distribution and its version.

	Properties: DIMbs

*DocDir:* _PATH_
	Declare a non-default documentation directory for the package.
	Rarely needed.

	Properties: ???t

*Enhances*[*(*_MODIFIERS_*)*]*:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*Epoch*: _N_
	Artificial versioning override. See *rpm-version*(7).

	Properties: Dbs

	```
	Epoch: 1
	```

*ExcludeArch:* _ARCH_ ...
	Package is not buildable on architectures listed here.
	Used when software is portable across most architectures except some,
	for example due to endianness issues.

	Properties: t

	```
	ExcludeArch: s390x
	```

*ExclusiveArch:* _ARCH_ ...
	Package is only buildable on architectures listed here.
	Used for software that is inherently architecture specific.

	Properties: t

	```
	ExclusiveArch: s390x
	```

*ExcludeOS:* _OS_ ...
	Package is not buildable on operating systems listed here.

	Properties: t

	```
	ExcludeOS: AIX
	```

*ExclusiveOS:* _OS_ ...
	Package is only buildable on operating systems listed here.

	Properties: t

	```
	ExclusiveOS: Linux
	```

*Group*[*(*_LANG_*)*]*:* _TEXT_
	Group of the package.

	Properties: DIbs

	```
	Group: Development/Libraries
	```

*License:* _TEXT_
	Short summary of the package license.
	Use of SPDX license identifiers is recommended.

	Properties: DIRbs

	```
	License: GPLv3-only
	```

*ModularityLabel:* _TEXT_
	Obsolete.

	Properties: DIMObs

*Name:* _NAME_
	The full name of the package.
	Permitted characters are alphanumerics and *-*, *\_*, *.*, *+*.

	Properties: DRXbs

	```
	Name: mypackage
	```

*NoPatch*: _N_ ...
	Exclude patch number _N_ from being included in source RPMs
	generated from this spec. Causes the source RPM to be called *nosrc*.

	Properties: t

	```
	Patch5: proprietary-tool-3.0-memleak.patch
	NoPatch: 5
	```

*NoSource*: _N_ ...
	Exclude source number _N_ from being included in source RPMs
	generated from this spec. Causes the source RPM to be called *nosrc*.

	Properties: t

	```
	Source1: proprietary-tool-3.0.zip
	Source2: proprietary-data-3.0.zip
	NoSource: 1 2
	```

*Obsoletes:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: bs

	```
	Obsoletes: oldtool < 3.0
	```

*OrderWithRequires:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*Packager:* _TEXT_
	Maintainer contact information.

	Properties: DIMbs

*Patch*[_N_]*:* [_URL_/]_FILE_
	Used to declare patches applied on top of sources.
	All declared patches are included in a source RPM, unless excluded
	with *NoPatch*. The _URL_ part is ignored, _FILE_ is interpreted
	as a file in the *%{SOURCEDIR}* directory.

	Patch numbers do not need to be consecutive and may include leading
	zeroes. Non-numbered patch tag *Patch:* is also supported and is
	automatically assigned the next available patch integer.

	Patch numbers are separate from source numbers.

	Properties: Ds

	See also *%patchlist*.

*PreReq:* _CAPABILITY_ ...
	Obsolete. Use *Requires(pre,postun)* for a similar effect.

	Properties: Ob

*Prefix*: _PATH_
	Compatibility alias for *Prefixes*.

	Properties: Db

*Prefixes*: _PATH_ ...
	Specify relocatable prefixes in this package.
	Such prefixes can be relocated to user-specified directories
	with *--relocate* option of *rpm*(8) at install time. The
	prefix information is also available to *rpm-scriptlets*(7).

	Note: most software will only work correctly in the path
	specified during build.

	Properties: Db

	```
	Prefixes: /usr /etc
	```

*Provides:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*Recommends*[*(*_MODIFIERS_*)*]*:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*Release:* _VERREL_
	The downstream packaging revision of a software version.
	See *rpm-version*(7).

	Properties: DRSbs

	```
	Release: 1
	```

*RemovePathPostfixes:* _POSTFIX1_[:_POSTFIX2_ ...]
	Colon separated lists of path postfixes that are removed from the
	end of filenames when adding those files to the package.
	Used on sub-package level.

	Used for creating sub-packages with conflicting files, such as
	different variants of the same content (eg minimal and full versions
	of the same software).

	Properties: t

	```
	%package minimal
	RemovePathPostfixes: .minimal

	%package full
	RemovePathPostfixes: .full
	```

*Requires*[*(*_MODIFIERS_*)*]*:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*Source*[_N_]*:* [_URL_/]_FILE_
	Used to declare source(s) used to build the package.
	All declared sources are included in a source RPM, unless excluded
	with *NoSource*.  The _URL_ part is ignored, _FILE_ is interpreted
	as a file in the *%{SOURCEDIR}* directory.

	Arbitrary number of sources may be declared, for example:

	```
	Source0: mysoft-1.0.tar.gz
	Source1: mysoft-data-1.0.zip
	```

	Source numbers do not need to be consecutive and may include leading
	zeroes. Non-numbered source tag *Source:* is also supported and is
	automatically assigned the next available source integer.

	Source numbers are separate from patch numbers.

	Properties: Ds

	See also *%sourcelist*.

*SourceLicense*: _TEXT_
	Short summary of the source license if it differs
	from the binary package license.

	Use of SPDX license identifiers is recommended.

	Properties: DIs

	```
	License: GPLv3-only
	```

*Suggests*[*(*_MODIFIERS_*)*]*:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*Summary*[*(*_LANG_*)*]*:* _TEXT_
	Short summary of the package.

	Properties: DIRbs

	```
	Summary: Utility for converting mumbles into giggles
	```

*Supplements*[*(*_MODIFIERS_*)*]*:* _CAPABILITY_ ...
	See *rpm-dependencies*(7).

	Properties: b

*TranslationURL:* _URL_
	URL of upstream translation service/repository.

	Properties: DIMbs

*UpstreamReleases:* _URL_
	URL to upstream releases.

	Properties: DIMbs

*URL:* _URL_
	URL supplying further information about the package, typically the
	project upstream website.

	Properties: DIbs

	```
	URL: https://rpm.org
	```

*VCS:* _TEXT_
	Upstream source code version control system location.

	Properties: Ibs

*Vendor:* _TEXT_
	Vendor name.

	Properties: DIMbs

*Version:* _VERREL_
	The version of the package, typically the (upstream) software version.
	When the version changes, *Release* should be reset.
	See *rpm-version*(7).

	Properties: DRbs

# SECTIONS

*%changelog*
	The changelog is for describing changes to the spec over time.
	A copy of the changelog is included in all packages built
	from the spec.

	The general form of the *%changelog* is a header line starting
	with *\**, followed by one or more message lines starting with
	*-*:

	Two forms of a header line are supported:

	_WEEKDAY_ _MONTH_ _DAY_ _YEAR_ _EMAIL_

	_WEEKDAY_ _MONTH_ _DAY_ _TIME_ _TZ_ _YEAR_ _EMAIL_

	_WEEKDAY_ is the three letter abbreviation of day of week in English.
	_MONTH_ is the three  letter abbreviation of month name in English.
	_DAY_ is the day of month, _YEAR_ is the year. If specified,
	_TIME_ is in 24h format and _TZ_ is relative UTC. _EMAIL_ is the
	standard "First Lastname <user@domain>" email contact format.

	Properties: bs

	```
	* Mon May 11 2026 Joe Doe <joedoe@example.org>
	- split libraries into their own sub-package

	* Thu May 7 17:05:00 UTC+2 Joe Doe <joedoe@example.org>
	- initial packaging
	- working overtime, see?
	```

*%description* [*-l* _LANG_] [*-n*] [_SUBNAME_]
	A multiline description of the (sub-)package for human audiences.
	Required for all packages.

	*-l* _LOCALE_
		Description is for *locale*(7) _LANG_.

	*-n*
		Interpret _SUBNAME_ as the full package name.

	Properties: bs

	```
	%description devel
	Development libraries and headers for %{NAME}.
	```

*%files* [*-n*] [_SUBNAME_] [*-f* _FILE_ [*-f* _FILE_ ...]]
	A manifest describing the files in the (sub-)package.

	*-f* _FILE_
		Additional files manifest to be read from disk after
		the build scripts have completed.
		May be supplied multiple times.

	*-n*
		Interpret _SUBNAME_ as the full package name.

	Binary packages are only produced for packages with a *%files*
	section, whether they contain any files or not. This is useful
	for conditional sub-package generation: only the *%files*
	section needs to be conditionalized.

	Properties: b

	The section format is one *glob*(7) entry per line, optionally preceded
	by attribute directives that apply to all the matching files:

	\[_ATTRIBUTE_ ...] _GLOB_

	The attributes fall roughly into three categories: concrete,
	semantic and verification attributes:

	*%artifact*
		Denotes files that are more like side-effects of packaging
		than actual content the user would be interested in. Such
		files can be easily filtered out on queries and also left
		out of installations if space is tight.

		Added: 4.14.1

	*%attr(*_MODE_*,* _USER_*,* _GROUP_*)*
		Override the permissions for a single _GLOB_. _MODE_ is
		an octal number such as you'd pass to *chmod*(1). _USER_
		and _GROUP_ are user and group names such as you'd pass
		to *chown*(1). Non-root _USER_ and _GROUP_ cause additional
		dependencies to be added to the package, see *rpm-sysusers*(7).

		Parameters may be omitted by specifying them as *-*,
		in which case the default value is used for that parameter.

	*%ghost*
		Indicates that this file is not to be included in the package.
		It is typically used when the attributes of the file are
		important while the contents is not (e.g. a log file).

	*%config*[*(*_MODIFIERS_*)*]
		Indicates a configuration file whose contents need tracking
		for local changes.

		Supported %config _MODIFIERS_ are:
		- *missingok*: Non-present file does not cause verify failure.
		- *noreplace*: Don't replace a locally modified file.

		If a *%config* file replaces an existing file on install,
		the original file is saved with *.rpmorig* suffix. A
		modified *%config* file is backed up with *.rpmsave*
		suffix on erase. A locally modified *%config(noreplace)* is
		preserved as-is on package updates, but the new content
		from the package is saved with *.rpmnew* suffix for reference.

	*%defattr(*_MODE_*,* _USER_*,* _GROUP_[*,* _DIRMODE_]*)*
		Set the default permissions and ownership for any following
		_GLOB_ entries in the manifest. _MODE_, _USER_ and _GROUP_
		are identical to the corresponding *%attr()* parameters,
		_DIRMODE_ is identical to _MODE_ but used for any directory
		entries.

	*%dir*
		Used to explicitly own the directory itself but not its
		contents.

	*%doc*
		Used to mark and/or install files as documentation.

		Can be used as a regular attribute on an absolute path, or
		in “special” form on a path relative to the build directory
		which causes the files to be installed and packaged as
		documentation. The special form strips all but the last path
		component. Thus *%doc path/to/docfile* installs docfile in the
		documentation path.

	*%license*
		Used to mark and/or install files as licenses.

		Similar to *%doc* and can be used with absolute or "special"
		forms.

		Added: 4.11.0

	*%missingok*
		Used to mark file presence optional, ie one whose absence
		does not cause verify to fail.

		Added: 4.14.0 (in standalone form)

	*%readme*
		Used to mark a README file.

	*%verify*[*(*_MODIFIERS_*)*]
		Tell *-–verify* to ignore certain features on files.
		Used to avoid false positives from files that are expected
		to be modified in certain ways.

		Supported modifiers are:
		- *filedigest*
		- *size*
		- *link*
		- *user* (or *owner*)
		- *group*
		- *mtime*
		- *mode*
		- *rdev*
		- *caps*

		```
		%verify(not size filedigest mtime) %{prefix}/bin/javaswarm
		```

	*%caps(*_CAPABILITY_*)*
		Sets the given POSIX.1e draft 15 *capabilities*(7) on the file.
		_CAPABILITY_ is the textual representation of capability sets
		as described in *cap_text_formats*(7).

		This feature is only available if RPM was built with libcap
		support. On Linux, file capabilities are only available since
		the kernel version 2.6.24. Many filesystems (such as NFS) do
		not support capabilities at all, which can cause install-time
		failures and/or incorrectly functioning packages when such
		filesystems are in use.

		```
		%caps(cap_net_raw=p) %{_bindir}/ping
		```

	_GLOB_
		The usual rules for shell globbing (see *glob*(7)) apply,
		including brace expansion. Metacharacters can be escaped by
		prefixing them with a backslash (*\\​*). Spaces are used to
		separate filenames and must also be escaped.

		Enclosing a filename in double quotes (*"*) preserves the
		literal value of all characters within the quotes, with the
		exception of *\\​* and the percent sign (*%*). A *\\​*
		or *%* can be escaped with an extra *\\​* or *%*,
		respectively. A double quote can be escaped with a *\\​*.

		If a glob pattern has no matches, it is tried literally (as
		if all the metacharacters were escaped). This is similar to
		how *bash*(1) works with the *failglob* option unset.

	By default, package files and directories are owned by root:root and
	their permission bits are taken from the on-disk files, ie. an
	implicit *%defattr(-,root,root)*.

	For example:

	```
	%doc README
	%license COPYING

	%config(noreplace) %{_sysconfdir}/my.conf
	%{_bindir}/myprog
	%{_libdir}/%{name}/my*.so.*
	%{_mandir}/man1/myprog.1*

	/opt/are.you|bob\?
	/opt/bob's\*htdocs\*
	/opt/bob's%%htdocs%%
	"/opt/bob's htdocs"
	```

*%package* [*-n*] _SUBNAME_
	Declare a sub-package.
	See *PREAMBLE* for the syntax and accepted tags.

	By default, _SUBNAME_ sub-package
	names are formed by appending _SUBNAME_ to the main package name,
	separated by dash: _NAME_*-*_SUBNAME_.

	*-n*
		Interpret _SUBNAME_ as the full package name.

	Properties: b

	```
	Name: myproj
	Summary: main binaries for myproj
	...
	
	%package libs
	Summary: shared libraries for myproj
	```

*%patchlist*
	List of patch files, one per line.

	Identical to using non-numbered *Patch:* lines.

	Properties: s

	```
	%patchlist
	myproj-1.0-memleak.patch
	myproj-1.0-crashbug.patch
	```

*%sourcelist*
	List of source files, one per line.

	Identical to using non-numbered *Source:* lines.

	Properties: s

	```
	%sourcelist
	mysoft-1.0.tar.gz
	mysoft-data-1.0.zip
	```

## Transaction scriptlets

Transaction scriplets are used to execute actions at pre-determined times of
a binary package lifetime, such as just after install or just before
uninstall.

See *rpm-scriptlets*(7).

*%filetriggerin*++
*%filetriggerun*++
*%filetriggerpostun*++
*%pre*++
*%pretrans*++
*%preuntrans*++
*%preun*++
*%post*++
*%posttrans*++
*%postun*++
*%postuntrans*++
*%transfiletriggerin*++
*%transfiletriggerpostun*++
*%transfiletriggerun*++
*%triggerprein*++
*%triggerin*++
*%triggerun*++
*%triggerpostun*++
*%verify*

	Properties: b

## Build scriptlets
Build scriptlet sections are literal *sh*(1) scripts that are executed during
different stages of a package build, from unpacking the source through
configuring, building and testing it to placing the files into their target
directories.

All build sections are optional - the packaged software defines the needs.

There are numerous helper macros for packaging purposes that are only
available/meaningful in spec files, see *rpm-spec-macros*(7).

Each build scriptlet section may be present only once, but it's possible
to augment them multiple times by using the following options:
- *-a*: append to the section
- *-p*: prepend to the section

The augment operations are applied relative to the corresponding main section,
in the order they appear in the spec. If the main section does not exist,
they are applied relative to the first fragment.

During the execution of build scriptlets, the following
RPM-specific environment variables are set:
- *RPM_ARCH*: The architecture of the package
- *RPM_BUILD_DIR*: The build directory of the package
- *RPM_BUILD_NCPUS*: The number of CPUs available for the build
- *RPM_BUILD_ROOT*: The buildroot directory of the package
- *RPM_BUILD_TIME*: The build time of the package (seconds since the epoch)
- *RPM_DOC_DIR*: The special documentation directory of the package
- *RPM_LD_FLAGS*: Linker flags
- *RPM_OPT_FLAGS*: Compiler flags
- *RPM_OS*: OS of the package
- *RPM_PACKAGE_NAME*: Rpm name of the source package
- *RPM_PACKAGE_VERSION*: Rpm version of the source package
- *RPM_PACKAGE_RELEASE*: Rpm release of the source package
- *RPM_SOURCE_DIR*: The source directory of the package
- *RPM_SPECPARTS_DIR*: The directory of dynamically generated spec parts

Note: many of these have macro counterparts which may seem more convenient
and consistent with the rest of the spec, but one should always use the
environment variables inside the scripts. The reason for this is that macros
are evaluated during spec parse and may not be up-to-date, whereas
environment variables are evaluated at the time of their execution in the
script.

*%prep*
	Prepares the sources for building. This is where sources are unpacked,
	possible patches applied and other similar pre-flight activities
	performed.

	Properties: t

	For example:
	```
	%prep
	%autosetup -p1
	```

*%generate_buildrequires*
	Used to determine build dependencies dynamically. If present, it is
	executed after *%prep* and can thus access the unpacked and patched
	sources. The script must print the found build dependencies to standard
	output in the same syntax as *BuildRequires* use, one dependency per
	line.

	Properties: t

	See *DYNAMIC BUILD DEPENDENCIES* in *rpmbuild*(1) for more information.

	Added: 4.15.0

*%conf*
	Used to configure the prepared sources for building, such as running
	*cmake*(1) or the _configure_ script from an *autoconf*(1) based build.

	Properties: t

	For example:
	```
	%conf
	%configure \
		--with-foo \
		--without-bar
	```

	Added: 4.18.0

*%build*
	Used to compile the configured sources to binaries.

	Properties: t

	For example:
	```
	%build
	%make_build
	```

*%install*
	Used to install the compiled binaries to their target locations inside
	the build root directory.

	Properties: t

	```
	%install
	%make_install
	```

*%check*
	Used to run software self-tests where present.

	Properties: t

*%clean*
	Obsolete. Packages should place any temporaries inside their
	designated *%builddir*, which RPM will automatically clean up.

	Used to clean up temporary files after the build.

	Properties: Ot

# CONDITIONALS
The spec supports several conditionals which can appear anywhere in the
spec, and can nest.

Caution should be exercised in using conditionals. They make the spec
harder to read, and can cause unwanted side-effects.
For example, conditionalizing a patch declaration can cause the patch to be
omitted from the source archive, creating an incomplete source package which
cannot be rebuilt in all situations otherwise supported by the spec.

*%if* _EXPRESSION_
	Begins a free-form conditional.

*%ifarch* _ARCH_ ...
	Begins an architecture conditional: enter block if building on
	one of the _ARCH_ architectures.

*%ifnarch* _ARCH_ ...
	Begins an architecture conditional. Reverse of *ifarch*: enter
	block if not building on one of the _ARCH_ architectures.

*%ifos* _OS_ ...
	Begins an operating system conditional: enter block if building on
	one of the _OS_ operating systems.

*%ifnos* _OS_ ...
	Begins an operating system conditional. Reverse of *%ifnos*: enter
	block if not building on one of the _OS_ operating systems.

*%elif* _EXPRESSION_
	Same as *%if*, but enter block if previous conditional was false.

*%elifarch* _ARCH_ ...
	Same as *%ifarch*, but enter block if previous conditional was false.

*%elifos* _OS_ ...
	Same as *%ifos*, but enter block if previous conditional was false.

*%else*
	Final else-clause, entered if no other conditional was true.

*%endif*
	Ends a conditional block for all the *%if* variants.

# ARGUMENTS

_ARCH_
	A computer architecture label known to RPM. See *rpm-rpmrc*(5).

_BOOLEAN_
	{*1*|*0*|*yes*|*no*}

_CAPABILITY_
	See *rpm-dependencies*(7).

_EXPRESSION_
	See *Expression expansion* in *rpm-macros*(7).

_FILE_
	A relative filename, typically just the basename component.

_LANG_
	*locale*(7) language name.

_MODIFIERS_
	A comma-separated list of (context-specific) modifier names.

_MULTILINE_
	Free-form multi-line text.

_N_
	A natural number in the set of {0, 1, 2, ...}.

_NAME_
	The name part of a _CAPABILITY_. See *rpm-dependencies*(7).

_OS_
	An operating system label known to RPM. See *rpm-rpmrc*(5).

_PATH_
	An absolute pathname.

_TEXT_
	Free-form line of text, preferably under 70 characters to
	avoid line-wrapping in common usage contexts.

_URL_
	A uniform resource locator (URL), colloquially known as a web address.

_VERREL_
	A string conforming to the restrictions of *Version* and *Release*
	*rpm-version*(7) components.

# EXAMPLES
## Example 1. A minimal arch-independent spec
```
Name: mini
Version: 1.0
Release: 1
License: MIT
Summary: Minimal spec
BuildArch: noarch

%description
A minimal spec file.

%files
```

## Example 2. A simple hello-world package
```
Name: hello
Version: 2.0
Release: 1
License: MIT
Summary: Hello world demo package
Source: https://github.com/rpm-software-management/rpm/blob/master/tests/data/SOURCES/hello-2.0.tar.gz

%description
The purpose of this package is to demonstrate RPM packaging.
It has no real-world use value beyond education and testing.
%end

# unpack sources, without %end this comment would end up in description
%prep
%autosetup

%conf
%configure

%build
# make -j${RPM_BUILD_NCPUS}
%make_build

%install
# make install DESTDIR=${RPM_BUILD_ROOT}
%make_install

%files
%{_bindir}/hello

%changelog
* Mon May 11 2026 Joe Doe <joedoe@example.org>
- Initial packaging
```

# SEE ALSO
*rpmbuild*(1) *rpm-version*(7) *rpm-config*(5) *rpmbuild-config*(5) *rpmspec*(1)
*rpm-lua*(7) *rpm-macros*(7) *rpm-spec-macros*(7) *rpm-dependencies*(7)
*rpm-sysusers*(7) *rpm-scriptlets*(7) *rpm-dependency-generators*(7)
*rpm-design*(7) *rpm*(8)

*http://www.rpm.org/*
