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
an automated and repeateple manner.

A _package_ consists of an archive of files and meta-data used to
install and erase the archive files. The meta-data includes helper
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
	Build from a spec file.

*-r*​_STAGE_
	Build from a source RPM package.

*-t*​_STAGE_
	Build from a *tar*(5) archive.

*--rebuild*,
*--recompile*
	Compatibility aliases for *-ra*.

Packages are built in a number of _stages_ that can be further divided
into categories: parse, build, assembly and cleanup. Assembly stages are
those that produce packages, the others are mainly of interest to developers
packaging new software in the RPM format, and troubleshooting. Build stages
generally correspond to sections like *%prep* and *%build* in a spec file,
but others are implicit in the build process.

Building a _STAGE_ involves building all the preceding stages before it.

## Assembly
*a*
	Build both source and binary packages.
	This is not an actual stage but a combination of *b* and *s*,
	with the difference that packages built this way carry a "cookie" to
	indicate they came from the same build.

	On success, the build directory is removed.

*b*
	Build just the binary packages, executing all the preceding stages.

	On success, the build directory is removed.

*r*
	Build just the source package, checking for dynamic build dependencies.
	Executes *%prep* and *%generate_buildrequires* stages before creating
	a package. See the *DYNAMIC BUILD DEPENDENCIES* section for details.

*s*
	Build just the source package. No build stages are executed.

## Build stages
The build stages are listed here in the order of execution, with
the corresponding spec section in parenthesis where applicable.

*p* (%prep)
	Unpack the sources and apply any patches - executes the *%prep* stage
	only.

*d* (%generate_buildrequires)
	Check dynamic build dependencies and build the _buildreqs.nosrc.rpm_
	package if any are missing. Don't build anything else.

*f* (%conf)
	Configure the sources - executes up to and including the %conf stage.
	This generally involves the equivalent of a *./configure*.

*c* (%build)
	Compile the sources - executes up to and including the *%build* stage.
	This generally involves the equivalent of *make*.

*i* (%install and %check)
	Install the binaries into the build root - executes up to and
	including the *%check* stage. This generally involves the equivalent
	of a *make install* and *make check*.

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
On success, 0 is returned, a non-zero failure code otherwise.

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

# SEE ALSO
*gendiff*(1), *popt*(3), *rpm*(8), *rpm-common*(8), *rpmbuild-config*(5),
*rpm2cpio*(1), *rpmkeys*(8), *rpmspec*(1), *rpmsign*(1),
*rpm-setup-autosign*(1) *rpm-macros*(7)

*http://www.rpm.org/*
