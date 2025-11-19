RPMBUILD(1)

# NAME
rpmbuild - Build RPM Package(s)

# SYNOPSIS
*rpmbuild* *-b*​_STAGE_ [options] _SPEC_FILE_ ...

*rpmbuild* *-r*​_STAGE_ [options] _SOURCE_PACKAGE_ ...

*rpmbuild* *-t*​_STAGE_ [options] _TAR_ARCHIVE_ ...

*rpmbuild* {*--rebuild*|*--recompile*} [options] _SOURCE_PACKAGE_ ...

# DESCRIPTION
*rpmbuild* is used to build software packages in the RPM format, in
an automated and repeatable manner.

An RPM package file (or _package_) consists of an archive of files and metadata
used to install or erase the archived files. The metadata includes helper
scripts, file attributes, and descriptive information about the package.

Packages come in two varieties: binary packages, used to encapsulate
software to be installed, and source packages, containing the source
code and recipe necessary to produce binary packages.

; scdoc doesn't currently render underline adjacent to bolding, so we need
; a little unicode hack to put something invisible between the operation
; letter and STAGE: the U200b character works nicely for the purpose.
; vim shows this as <200b> with other editors, YMMV.
# OPERATIONS
*-b*​_STAGE_
	Build _STAGE_ from a spec file.

*-r*​_STAGE_
	Build _STAGE_ from a source RPM package.

*-t*​_STAGE_
	Build _STAGE_ from a *tar*(5) archive.

*--rebuild*,
*--recompile*
	Compatibility aliases for *-ra*.

_STAGE_ is a single-letter argument that specifies the target stage to build.
This also executes all preceding stages unless stated (or configured) otherwise.
Additional steps beyond the one specified, such as clean up, may also execute.
See *BUILD PROCESS* and *OPTIONS* for details.

## Building packages

Use these _STAGE_ arguments to produce RPM package files ready for installation
and/or distribution.

*a*
	Build both source and binary packages.
	This is not an actual stage but a combination of *b* and *s*,
	with the difference that packages built this way carry a "cookie" to
	indicate they came from the same build.
	On success, the build directory is removed.

*b*
	Build just the binary packages.
	On success, the build directory is removed.

*r*
	Build just the source package, checking for dynamic build dependencies.
	Executes the *%prep* and *%generate_buildrequires* stages before
	creating a package. See the *DYNAMIC BUILD DEPENDENCIES* section for
	details.

*s*
	Build just the source package. No build stages are executed.

## Partial builds

Use these _STAGE_ arguments to build or test the encapsulated software without
producing RPM package files. Useful when packaging new software in RPM format
and/or troubleshooting.

The corresponding spec sections are shown in parentheses where applicable.

*p* (%prep)
	Unpack the sources and apply any patches.

*d* (%generate_buildrequires)
	Check dynamic build dependencies and build the _buildreqs.nosrc.rpm_
	package if any are missing. Don't build anything else.

*f* (%conf)
	Configure the sources. This generally involves the equivalent of
	*./configure*.

*c* (%build)
	Compile the sources. This generally involves the equivalent of *make*.

*i* (%install)
	Install the binaries into the build root. This generally involves the
	equivalent of a *make install*.

	For historical reasons, this also includes the %check stage by default.

*k* (%check)
	Run any checks specified in the spec. This generally involves the
	equivalent of *make check*.

*l*
	Do a "list check" - the *%files* section from the spec file is macro
	expanded, and checks are made to verify that each file exists.
	This requires a previous build up to the *%install* stage to have
	taken place.

# ARGUMENTS
_SPEC_FILE_
	An RPM spec file.

_SOURCE_PACKAGE_
	An RPM source package (with a _.src.rpm_ extension)

_TAR_ARCHIVE_
	A *tar*(5) archive, optionally compressed. To be directly buildable
	with *rpmbuild*, an archive must contain a spec file either by
	the name _Specfile_ or one with a _.spec_ extension.

# OPTIONS
*--build-in-place*
	Build from locally checked out sources in the current working
	directory. The build tree is set up as if *%setup* was used,
	but *%builddir*/*%buildsubdir* points back to the current working
	directory. *%prep* is skipped entirely.

*--clean*
	Remove the build tree after the packages are made (default).

*--nobuild*
	Do not execute any build stages. Useful for testing out spec files.

*--nocheck*
	Do not execute *%check* build stage even if present in spec.

*--noclean*
	Do not execute *%clean* build stage even if present in spec.

*--nodebuginfo*
	Do not generate debuginfo packages.

*--nodeps*
	Do not verify build dependencies.

*--noprep*
	Do not execute *%prep* build stage even if present in spec.
	This assumes there has been another *rpmbuild* run in which
	*%prep* has been already executed.

*--rmsource*
	Remove the sources after the build (may also be used standalone,
	e.g. *rpmbuild* *--rmsource foo.spec*).
	Note that by definition, *NoSource* and *NoPatch* files are not
	sources and so, are not affected by this option.

*--rmspec*
	Remove the spec file after the build (may also be used standalone,
	e.g. *rpmbuild* *--rmspec foo.spec*).

*--rpmfcdebug*
	Print debug information on file classification and dependency
	generation.

*--scm*=_SCM_
	Select the _SCM_ to use with *%autosetup*, if one is not set in the
	spec file. Note that not all values for _SCM_, e.g., *patch* (the
	default) and *gendiff*, *git*, or *quilt* work interchangeably
	with all other patches and options stated in the %autosetup line,
	especially option *-p**N*.

*--short-circuit*
	Skip straight to specified stage (i.e., skip all stages leading up
	to the specified stage). Only valid with *-bc*, *-bi*, and
	*-bb*. Useful for local testing only. Packages built this way will
	be marked with an unsatisfiable dependency to prevent their
	accidental use.

*--with* _OPTION_
	Enable configure _OPTION_ for build.

*--without* _OPTION_
	Disable configure _OPTION_ for build.

See *rpm-common*(8) for the options common to all operations.

# BUILD PROCESS

Packages are built in a number of logical steps, outlined below. The steps are
executed from top to bottom, skipping any that aren't applicable to the spec.

Those steps that produce (intermediate) build artifacts or diagnostic output are
called _stages_. They are denoted by a single-letter identifier in parentheses.
You can tell *rpmbuild* to stop the build process at any stage (see *OPERATIONS*
for details).

Some steps are implemented as build scriptlets whose contents is dumped in the
*rpmbuild* output for diagnostic purposes. Their names are shown in parentheses
following any stage identifier, prefixed with *%* if they correspond to a spec
section or directive.

Binary packages are produced separately for one or more platforms, called _build
targets_. By default, the current platform is the only build target. This can be
overridden with *--target* or by declaring a *BuildArch* in the spec. See also
*BUGS* for some related known issues.

- Unpack source package or tar archive if either is given
- Determine build targets and repeat following steps for each in turn
- Load *rpm-config*(5), *rpm-rpmrc*(5) and *rpmbuild-config*(5) files for next
  build target
- Parse spec file
- Check static build dependencies
- Setup build workspace
	- Create build directory and set *%builddir* to its path (*mkbuilddir*)
	- Export build environment to *%builddir*/_rpmbuild.env_ file
- Execute build scriptlets to populate *%buildroot*
	- Unpack sources and apply any patches (*p*, *%prep*)
	- Generate dynamic build dependencies (*d*, *%generate_buildrequires*)
	- Check dynamic build dependencies
	- Configure sources (*f*, *%conf*)
	- Compile sources (*c*, *%build*)
	- Install binaries into *%buildroot* (*i*, *%install*)
	- Read dynamic spec parts
	- Execute tests (*k*, *%check*)
- Process files to be packaged
	- Populate file list for source packages
	- Populate file list for binary packages based on *%files* manifest(s)
		- Add regular entries from *%buildroot*
		- Copy special entries to *%buildroot* (*%doc*, *%license*)
		- Add special entries from *%buildroot*
	- Perform file classification
	- Generate runtime dependencies using *rpm-dependency-generators*(7)
	- Check file list against *%buildroot* (*l*)
- Create packages
	- Write binary packages to disk (*b*)
	- Write source packages to disk (*s*)
- Clean up
	- OBSOLETE: Perform explicit clean up (*%clean*)
	- Perform implicit clean up (*rmbuild*)

For more information on the syntax, see *rpm-spec*(5).

# DYNAMIC BUILD DEPENDENCIES
When the *%generate_buildrequires* stage runs and some of the newly
generated BuildRequires are not satisfied, *rpmbuild* creates an
intermediate source package ending in _buildreqs.nosrc.rpm_, which has
the new BuildRequires, and exits with code 11. This package can then be
used in place of the original source package to resolve and install the
missing build dependencies in the usual way, such as with
*dnf-builddep*(8).

Multiple layers of dynamic build dependencies may exist in a spec file;
the presence of specific BuildRequires on the system may yield new
BuildRequires next time a build is performed with the same source
package. The easiest way to ensure that all dynamic build dependencies
are satisfied is to run the *-br* option, install the new
dependencies of the _buildreqs.nosrc.rpm_ package and repeat the whole
procedure until *rpmbuild* no longer exits with code 11.

If the *-br* option is coupled with *--nodeps*, exit code 11 is
always returned and a _buildreqs.nosrc.rpm_ package is always created.

# ENVIRONMENT
*RPM_BUILD_NCPUS*
	Overrides autodetection for the number of CPUs to use for
	parallelized sections of the build.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

Packages with a *%generate_buildrequires* section will return with code 11
when there are unmet *DYNAMIC BUILD DEPENDENCIES*.

# EXAMPLES
*rpmbuild --rebuild hello-1.0-1.src.rpm*
	Build binary and source packages from the _hello-1.0-1.src.rpm_
	source package.

*rpmbuild -bb --nocheck --with openssl hello.spec*
	Build binary packages from the _hello.spec_ spec file,
	skipping the *%check* stage if present and enabling support for
	*openssl* build conditional (assuming one is specified in the spec).

*rpmbuild -ta hello-2.0.tar.gz*
	Build binary and source packages from the compressed
	_hello-2.0.tar.gz_ tarball, assuming the archive contains a
	legitimate spec file (see _TAR_ARCHIVE_ for details).

*rpmbuild -bc hello.spec*
	Build _hello.spec_ up to and including the *%build* stage,
	ie. without producing actual packages.

*rpmbuild -bi --short-circuit hello.spec*
	Only execute the *%install* stage of _hello.spec_, skipping all
	previous stages. This assumes a preceding run of at least up to
	the *%build* stage, see previous example. Useful to avoid full
	rebuilds when working on the *%files* section of a package.

# FILES
See *rpm-common*(8)

# BUGS

Building a spec that declares a *BuildArch* may cause unexpected behavior, in
particular:

- Macros defined before the *BuildArch* line will be expanded twice, which can
  yield unexpected results with *%global* in particular

- Using a *--target* that disagrees with the *BuildArch* results in undefined
  behavior

# SEE ALSO
*gendiff*(1), *popt*(3), *rpm*(8), *rpm-common*(8), *rpmbuild-config*(5),
*rpm2cpio*(1), *rpmkeys*(8), *rpmspec*(1), *rpmsign*(1),
*rpm-setup-autosign*(1) *rpm-macros*(7)

*http://www.rpm.org/*
