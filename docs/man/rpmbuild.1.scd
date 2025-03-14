RPMBUILD(1)

# NAME

rpmbuild - Build RPM Package(s)

# SYNOPSIS

The general form of *rpmbuild* command is

*rpmbuild* {*-b*|*-r*|*-t*}_STAGE_ [options] [build-options] _FILE_ ...

*rpmbuild* {*--rebuild*|*--recompile*} [options] [build-options] _SOURCEPKG_ ...

# DESCRIPTION

*rpmbuild* is used to build both binary and source software packages.
A *package* consists of an archive of files and meta-data used to
install and erase the archive files. The meta-data includes helper
scripts, file attributes, and descriptive information about the package.
*Packages* come in two varieties: binary packages, used to encapsulate
software to be installed, and source packages, containing the source
code and recipe necessary to produce binary packages.

One of the following basic modes must be selected: *Build Package*,
*Build Package from Tarball*, *Recompile Package*, *Show
Configuration*.

# OPTIONS

See *rpm-common*(8) for the options common to all operations.

# BUILD OPTIONS

The general form of an *rpm*(8) build command is

*rpmbuild* {*-b*|*-r*|*-t*}_STAGE_ [build-options] _FILE_ ...

The argument used is *-b* if a spec file is being used to build the
package, *-r* if a source package is to be rebuilt and *-t* if
*rpmbuild* should look inside of a (possibly compressed) tar file for
the spec file to use.

Packages are built in a number of stages. The first six correspond to
the following sections in a spec file: *%prep*,
*%generate_buildrequires*, *%build*, *%install*, *%check* and
*%clean*. Finally, binary and source packages are created in an
assembly stage.

The _STAGE_ character specifies the stage to finish with (after doing
all the stages preceding it), and is one of:

*-ba*
	Perform a full build - executes up to and including the assembly
	stage. In most cases, this is the option to choose.

*-bb*
	Build just the binary packages - executes up to and including the
	assembly stage, but without creating the source package.
	On success, the build directory is removed (as in *--clean*).

*-bp*
	Unpack the sources and apply any patches - executes the *%prep* stage
	only.

*-bf*
	Configure the sources - executes up to and including the %conf stage.
	This generally involves the equivalent of a "*./configure*".

*-bc*
	Compile the sources - executes up to and including the *%build* stage.
	This generally involves the equivalent of "*make*".

*-bi*
	Install the binaries into the build root - executes up to and
	including the *%check* stage. This generally involves the equivalent
	of a "*make install*" and "*make check*".

*-bl*
	Do a "list check" - the *%files* section from the spec file is macro
	expanded, and checks are made to verify that each file exists.

*-bs*
	Build just the source package - skips straight to the assembly
	stage, without executing any of the preceding stages or creating
	binary packages.

*-br*
	Build just the source package, but also parse and include dynamic
	build dependencies - executes up to and including the
	*%generate_buildrequires* stage and then skips straight to the
	assembly stage, without creating binary packages. This command can
	be used to fully resolve dynamic build dependencies. See the
	*DYNAMIC BUILD DEPENDENCIES* section for details.

*-bd*
	Check dynamic build dependencies and build the .buildreqs.nosrc.rpm
	package if any are missing. Don't build anything else.

The following options may also be used:

*--clean*
	Remove the build tree after the packages are made.

*--nobuild*
	Do not execute any build stages. Useful for testing out spec files.

*--noprep*
	Do not execute *%prep* build stage even if present in spec.

*--noclean*
	Do not execute *%clean* build stage even if present in spec.

*--nocheck*
	Do not execute *%check* build stage even if present in spec.

*--nodebuginfo*
	Do not generate debuginfo packages.

*--nodeps*
	Do not verify build dependencies.

*--rmsource*
	Remove the sources after the build (may also be used standalone,
	e.g. "*rpmbuild* *--rmsource foo.spec*").

*--rmspec*
	Remove the spec file after the build (may also be used standalone,
	e.g. "*rpmbuild* *--rmspec foo.spec*").

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

*--build-in-place*
	Build from locally checked out sources in the current working
	directory. The build tree is set up as if *%setup* was used,
	but *%builddir*/*%buildsubdir* points back to the current working
	directory. *%prep* is skipped entirely.

*--with* _OPTION_
	Enable configure _OPTION_ for build.

*--without* _OPTION_
	Disable configure _OPTION_ for build.

# REBUILD AND RECOMPILE OPTIONS

There are two other ways to invoke building with *rpmbuild*(1):

*rpmbuild* *--rebuild|--recompile* _SOURCEPKG_ ...

When invoked this way, *rpmbuild* installs the named source package,
and does a prep, compile and install. In addition, *--rebuild* builds
a new binary package. When the build has completed, the build directory
is removed (as in *--clean*) and the the sources and spec file for
the package are removed.

These options are now superseded by the *-r\** options which allow
much more fine control over what stages of the build to run.

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

If the *-br* command is coupled with *--nodeps*, exit code 11 is
always returned and a _buildreqs.nosrc.rpm_ package is always created.

# FILES

See *rpm-common*(8)

# SEE ALSO

*gendiff*(1), *popt*(3), *rpm*(8), *rpm-common*(8),
*rpm2cpio*(1), *rpmkeys*(8), *rpmspec*(1), *rpmsign*(1)

*rpmbuild --help* - as rpm supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
