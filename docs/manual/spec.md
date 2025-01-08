---
layout: default
title: rpm.org - Spec file format
---
# Spec file format

Spec files describe how software is build and packaged.

### Contents and Links

* [Generic syntax](#generic-syntax)
  * [→ Macro syntax](macros.md)
  * [Comments](#comments)
  * [Conditionals](#conditionals)
  * [→ Conditional Builds](conditionalbuilds.md)
  * [Sections](#sections)
* [Preamble](#preamble)
  * [Dependencies](#dependencies)
    * [→ Dependencies Basics](dependencies.md)
    * [→ More on Dependencies](more_dependencies.md)
    * [→ Boolean Dependencies](boolean_dependencies.md)
    * [→ Architecture Dependencies](arch_dependencies.md)
    * [→ Installation Order](tsort.md)
    * [→ Automatic Dependency Generation](dependency_generators.md)
  * [→ Declarative builds](buildsystem.md)
  * [→ Relocatable Packages](relocatable.md)
  * [Sub-sections](#sub-sections)
* [Build scriptlets](#build-scriptlets)
  * [→ Autosetup](autosetup.md)
* [Runtime scriptlets](#runtime-scriptlets)
  * [→ Triggers](triggers.md)
  * [→ File Triggers](file_triggers.md)
  * [→ Scriptlet Expansion](scriptlet_expansion.md)
* [%files section](#files-section)
  * [→ Users and Groups](users_and_groups.md)
* [%changelog section](changelog-section)

## Generic syntax

### Macros

Each line in the spec is macro-expanded before further processing. The macro
syntax and the built-in macros are described on a [dedicated page](macros).
Typically there are vast amounts of other macros available for helping with
common packaging tasks.

### Comments

Comments in spec file have # at the start of the line.

```
    # this is a comment
```

Comments are also allowed after conditionals (see below). Some older
versions of RPM (4.14 to 4.19) did issue a warning on those, but they
are fully legal from RPM 4.20 onward.

Macros are expanded even in comment lines. If this is undesireable, escape
the macro with an extra percent sign (%):

```
    # make unversioned %%__python an error unless explicitly overridden
```

Another option is to use built-in macro %dnl that discards text to next
line without expanding it. (since rpm >= 4.15)

```
    %dnl make unversioned %__python an error unless explicitly overridden
```

### Conditionals

RPM's spec file format allows conditional blocks of code to be used
depending on various properties such as architecture (%ifarch /%ifnarch),
operating system (%ifos / %ifnos), or a conditional expression (%if).

%ifarch is generally used for building RPM packages for multiple
platforms like:
```
	%ifarch s390 s390x
	BuildRequires: s390utils-devel
	%endif
```

%ifos is used to control RPM's spec file processing according to the
build target operating system.

%if can be used for various purposes. The test can be evaluated based on
the existence of a macro, like:
```
	%if %{defined with_foo} && %{undefined with_bar}
```
string comparison:
```
	%if "%{optimize_flags}" != "none"
```
or a mathematical statement:
```
	%if 0%{?fedora} > 10 || 0%{?rhel} > 7
```
Generally, a mathematical statement allows to use logical operators
`&&`, `||`, `!`, relational operators `!=`, `==`, `<`, `>` , `<=`, `>=`,
arithmetic operators `+`, `-`, `/`, `*`, the ternary operator `? :`,
and parentheses.

The conditional blocks end by %endif. Inside the conditional block %elif,
%elifarch, %elifos or %else can be optionally used. Conditionals %endif and
%else should not be followed by any text. Conditionals may be nested within
other conditionals.

%if-conditionals are not macros, and are unlikely to yield expected results
if used in them.


### Conditional Builds ###

Conditionals can be made available for users building the package.
[Conditional Builds](conditionalbuilds.md) add `--with` and `--without`
command line options to `rpmbuild` that can be used inside the spec
file.

### Sections ###

The spec file is divided in several sections. Except of the preamble
of the main package right at the start spec file (and spec parts)
sections begin with a percent sign and the name of the section. They
need to be at the start of a new line. Most section types allow
passing options in this first line. These section markers looks like
macros (with parameters) but are not.

Each section type has its own rules and syntax. Conditionals are
evaluated first and then macros expanded. Only then are the sections
parsed by the rules of the section types. The content of build and
runtime scripts is then passed on the the interpreter - possible being
stored in a header tag inbetween. The syntax of the other sections is
described below.

## Preamble

### Preamble tags

Since RPM 4.20 preamble tags can be indented with white space. Older
versions require the Tags to be at the beginning of a line. Comments
and empty lines are allowed.

#### Name

The Name tag contains the proper name of the package. Names must not
include whitespace and may include a hyphen '-' (unlike version and release
tags). Names should not include any numeric operators ('<', '>','=') as
future versions of rpm may need to reserve characters other than '-'.

#### Version

Version of the packaged content, typically software.

The version string consists of alphanumeric characters, which can optionally
be segmented with the separators `.`, `_` and `+`, plus `~` and `^` (see below).

Tilde (`~`) can be used to force sorting lower than base (1.1\~201601 < 1.1).
Caret (`^`) can be used to force sorting higher than base (1.1^201601 > 1.1).
These are useful for handling pre- and post-release versions, such as
1.0\~rc1 and 2.0^a.

#### Release

Package release, used for distinguishing between different builds
of the same software version.

See Version for allowed characters and modifiers.

#### Epoch

Optional numerical value which can be used to override normal version-release
sorting order. It's use should be avoided if at all possible.

Non-existent epoch is exactly equal to zero epoch in all version comparisons.

#### License

Short (< 70 characters) summary of the package license. For example:
```
	License: GPLv3
```

#### SourceLicense

If license of the sources differ from the main package the license tag
of the source package can be set with this. If not given the license
tag of the source and the main package are the same.

#### Group

Optional, short (< 70 characters) group of the package.
```
	Group: Development/Libraries
```

#### Summary

Short (< 70 characters) summary of the package.  
```
	Summary: Utility for converting mumbles into giggles
```

#### Source

Used to declare source(s) used to build the package. All sources will
will be packaged into source rpms.
Arbitrary number of sources may be declared, for example:

```
	Source0: mysoft-1.0.tar.gz
	Source1: mysoft-data-1.0.zip
```

Source numbers do not need to be consecutive and may include leading zeroes.
Unnumbered source tag `Source:` is also supported and is automatically assigned
the next available integer.

#### Patch

Used to declare patches applied on top of sources. All patches declared
will be packaged into source rpms. Just like sources, patches can be
numbered or unnumbered and are indexed the same way. Unless there is a
specific reason to use numbered patches, the recommended approach is to use
unnumbered patches and apply them using `%autosetup` or `%autopatch`.

#### Icon

Used to attach an icon to an rpm package file. Obsolete.

#### NoSource
#### NoPatch

Files ending in .nosrc.rpm are generally source RPM packages whose spec
files have one or more NoSource: or NoPatch: directives in them.  Both
directives use the named source or patch file to build the resulting
binary RPM package as usual, but they are not included in the source
RPM package.

The original intent of this ability of RPM was to allow proprietary or
non-distributable software to be built using RPM, but to keep the
proprietary or non-distributable parts out of the resulting source RPM
package, so that they would not get distributed.

They also have utility if you are building RPM packages for software
which is archived at a well-known location and does not require that
you distribute the source with the binary, for example, for an
organization's internal use, where storing large quantities of source
is not as meaningful.

The end result of all this, though, is that you can't rebuild
``no-source'' RPM packages using `rpm --rebuild' unless you also have
the sources or patches which are not included in the .nosrc.rpm.

#### URL

URL supplying further information about the package, typically upstream
website.

#### BugURL

Bug reporting URL for the package.

#### ModularityLabel
#### DistTag
#### VCS

#### Distribution
#### Vendor
#### Packager

Optional package distribution/vendor/maintainer name / contact information.
Rarely used in specs, typically filled in by buildsystem macros.

#### BuildRoot

Obsolete and unused in rpm >= 4.6.0, but permitted for compatibility
with old packages that might still depend on it.

Do not use in new packages.

#### Buildsystem

Automatically populate the spec build scripts for the given build system,
such as `Buildsystem: autotools". See [declarative build](buildsystem.md)
documentation for more details.

#### AutoReqProv
#### AutoReq
#### AutoProv

Control per-package automatic dependency generation for provides and requires. 
Accepted values are 1/0 or yes/no, default is always "yes". Autoreqprov is
equal to specifying Autoreq and Autoprov separately.

### Dependencies

The following tags are used to supply package dependency information,
all follow the same basic form. Can appear multiple times in the spec,
multiple values accepted, a single value is of the form
`capability [operator version]`. Capability names must
start with alphanumerics or underscore. Optional version range can be
supplied after capability name, accepted operators are `=`, `<`, `>`,
`<=` and `>=`, version 

#### Requires

Capabilities this package requires to function at all. Besides ensuring
required packages get installed, this is also used to order installs
and erasures. 

Additional context can be supplied using `Requires(qualifier)` syntax,
accepted qualifiers are:

* `pre`

  Denotes the dependency must be present in before the package is
  is installed, and is used a strong ordering hint to break possible
  dependency loops. A pre-dependency is free to be removed
  once the install-transaction completes.

  Also relates to `%pre` scriptlet execution.

* `post`

  Denotes the dependency must be present right after the package is
  is installed, and is used a strong ordering hint to break possible
  dependency loops. A post-dependnecy is free to be removed
  once the install-transaction completes.

  Also relates to `%post` scriptlet execution.

* `preun`

  Denotes the dependency must be present in before the package is
  is removed, and is used a strong ordering hint to break possible
  dependency loops.

  Also relates to `%preun` scriptlet execution.

* `postun`

  Denotes the dependency must be present right after the package is
  is removed, and is used a strong ordering hint to break possible
  dependency loops.

  Also relates to `%postun` scriptlet execution.

* `pretrans`

  Denotes the dependency must be present before the transaction starts,
  and cannot be satisified by added packages in a transaction. As such,
  it does not affect transaction ordering. A pretrans-dependency is
  free to be removed after the install-transaction completes.

  Also relates to `%pretrans` and `%preuntrans` scriptlet execution.

* `posttrans`

  Denotes the dependency must be present at the end of transaction, ie
  cannot be removed during the transaction. As such, it does not affect
  transaction ordering. A posttrans-dependency is free to be removed
  after the the install-transaction completes.

  Also relates to `%posttrans` and `%postuntrans` scriptlet execution.

* `verify`

  Relates to `%verify` scriptlet execution. As `%verify` scriptlet is not
  executed during install/erase, this does not affect transaction ordering.

* `interp`

  Denotes a scriptlet interpreter dependency, usually added automatically
  by rpm. Used as a strong ordering hint for breaking dependency loops.

* `meta` (since rpm >= 4.16)

  Denotes a "meta" dependency, which must not affect transaction ordering.
  Typical use-cases would be meta-packages and sub-package cross-dependencies
  whose purpose is just to ensure the sub-packages stay on common version.

Multiple qualifiers can be supplied separated by comma, as long as
they're not semantically contradictory: `meta` qualifier contradicts any
ordered qualifier, eg `meta` and `verify` can be combined, and `pre` and
`verify` can be combined, but `pre` and `meta` can not.

As noted above, dependencies qualified as install-time only (`pretrans`,
`pre`, `post`, `posttrans` or combination of them) can be removed after the
installation transaction completes if there are no other dependencies
to prevent that. This is a common source of confusion.

#### Provides

Capabilities provided by this package.

`name = [epoch:]version-release` is automatically added to all packages.

#### Conflicts

Capabilities this package conflicts with, typically packages with
conflicting paths or otherwise conflicting functionality.

#### Obsoletes

Packages obsoleted by this package. Used for replacing and renaming
packages.

#### Recommends (since rpm >= 4.13)
#### Suggests
#### Supplements
#### Enhances

#### OrderWithRequires (since rpm >= 4.9)

#### Prereq

Obsolete, do not use.

#### BuildPrereq

Obsolete, do not use.

#### BuildRequires

Capabilities required to build the package.

Build dependencies are identical to install dependencies except:

```
  1) they are prefixed with build (e.g. BuildRequires: rather than Requires:)
  2) they are resolved before building rather than before installing.
```

So, if you were to write a specfile for a package that requires gcc to build,
you would add
```
	BuildRequires: gcc
```
to your spec file.

If your package was like dump and could not be built w/o a specific version of
the libraries to access an ext2 file system, you could express this as
```
	BuildRequires: e2fsprofs-devel = 1.17-1
```

#### BuildConflicts

Capabilities which conflict, ie cannot be installed during the package
package build.

For example if somelib-devel presence causes the package to fail build,
you would add
```
	BuildConflicts: somelib-devel
```

#### ExcludeArch

Package is not buildable on architectures listed here.
Used when software is portable across most architectures except some,
for example due to endianess issues.

#### ExclusiveArch

Package is only buildable on architectures listed here.
For example, it's probably not possible to build an i386-specific BIOS
utility on ARM, and even if it was it probably would not make any sense.

#### ExcludeOS

Package is not buildable on specific OSes listed here.

#### ExclusiveOS

Package is only buildable on OSes listed here.

#### BuildArch (or BuildArchitectures)

Specifies the architecture which the resulting binary package
will run on.  Typically this is a CPU architecture like sparc,
i386. The string 'noarch' is reserved for specifying that the
resulting binary package is platform independent.  Typical platform
independent packages are html, perl, python, java, and ps packages.

As a special case, `BuildArch: noarch` can be used on sub-package
level to allow eg. documentation of otherwise arch-specific package
to be shared across multiple architectures.

Note that `BuildArch` causes the spec parsing to recurse from the start,
causing any macros before that line to be expanded twice. This can yield
unexpected results, in particular with `%global`.

#### Prefixes (or Prefix)

Specify prefixes this package may be installed into, used to make
packages relocatable. Very few packages are. See [Relocatable Packages](relocatable.md) for details.


#### DocDir

Declare a non-default documentation directory for the package.
Usually not needed.

#### RemovePathPostfixes

Colon separated lists of path postfixes that are removed from the end
of file names when adding those files to the package. Used on sub-package
level.

Used for creating sub-packages with conflicting files, such as different
variants of the same content (eg minimal and full versions of the same
software). 

#### More Dependencies related Topics
  * [Dependencies Basics](dependencies.md)
  * [More on Dependencies](more_dependencies.md)
  * [Boolean Dependencies](boolean_dependencies.md)
  * [Architecture Dependencies](arch_dependencies.md)
  * [Installation Order](tsort.md)
  * [Automatic Dependency Generation](dependency_generators.md)

### Sub-sections

#### `%sourcelist`

List of sources, one per line. Handled like unnumbered Source tags. For
clarity, mixing Source tags and `%sourcelist` in one specfile is not
recommended.

#### `%patchlist`

List of patches, one per line. Handled like unnumbered Patch tags. For
clarity, mixing Patch tags and `%patchlist` in one specfile is not
recommended.

#### `%package [-n]<name>`

`%package <name>` starts a preamble section for a new sub-package.
Most preamble tags can are usable in sub-packages too, but there are
exceptions such as Name, which is taken from the `%package` directive.

By default subpackages are named by prepending the main package name
followed by a dash to the subpackage name(s), ie `<mainname>-<subname>`.
Using the `-n` option allows specifying an arbitrary (sub-)package name.

#### `%description [-n][name]`

%description is free form text, but there are two things to note.
The first regards reformatting.  Lines that begin with white space
are considered "pre-formatted" and will be left alone.  Adjacent
lines without leading whitespace are considered a single paragraph
and may be subject to formatting by glint or another RPM tool.

The `-n` option and `<name>` are the same as for `%package`, except that
when name is omitted, the description refers to the main package.

## Build scriptlets

Package build is divided into multiple separate steps, each executed in a
separate shell: `%prep`, `%conf`, `%build`, `%install`, `%check`, `%clean`
and `%generate_buildrequires`. Any unnecessary scriptlet sections can be
omitted.

Each section may be present only once, but in rpm >= 4.20 it is
possible to augment them by appending or prepending to them using
`-a` and `-p` options.
Append and prepend can be used multiple times. They are applied relative
to the corresponding main section, in the order they appear in the spec.
If the main section does not exist, they are applied relative to the
first fragment.

During the execution of build scriptlets, (at least) the following
rpm-specific environment variables are set:

Variable            | Description
--------------------|------------------------------
RPM_ARCH            | Architecture of the package
RPM_BUILD_DIR       | The build directory of the package
RPM_BUILD_NCPUS     | The number of CPUs available for the build
RPM_BUILD_ROOT      | The buildroot directory of the package
RPM_BUILD_TIME      | The build time of the package (seconds since the epoch)
RPM_DOC_DIR         | The special documentation directory of the package
RPM_LD_FLAGS        | Linker flags
RPM_OPT_FLAGS       | Compiler flags
RPM_OS              | OS of the package
RPM_PACKAGE_NAME    | Rpm name of the source package
RPM_PACKAGE_VERSION | Rpm version of the source package
RPM_PACKAGE_RELEASE | Rpm release of the source package
RPM_SOURCE_DIR      | The source directory of the package
RPM_SPECPARTS_DIR   | The directory of dynamically generated spec parts

Note: many of these have macro counterparts which may seem more convenient
and consistent with the rest of the spec, but one should always use
the environment variables inside the scripts. The reason for this is
that macros are evaluated during spec parse and may not be up-to-date,
whereas environment variables are evaluated at the time of their execution
in the script.

### %prep

%prep prepares the sources for building. This is where sources are
unpacked and possible patches applied, and other similar activies
could be performed.

Typically [%autosetup](autosetup.md) is used to automatically handle
it all, but for more advanced cases there are lower level `%setup`
and `%patch` builtin-macros available in this slot.

In simple packages `%prep` is often just:
```
%prep
%autosetup
```

#### %setup

`%setup [options]`

The primary function of `%setup` is to set up the build directory for the
package, typically unpacking the package's sources but optionally it
can just create the directory. It accepts a number of options:

```
-a N        unpack source N after changing to the build directory
-b N        unpack source N before changing to the build directory
-c          create the build directory (and change to it) before unpacking
-C          Create the build directory and ensure the archive contents
            are unpacked there, stripping the top level directory in the archive
            if it exists
-D          do not delete the build directory prior to unpacking (used
            when more than one source is to be unpacked with `-a` or `-b`)
-n DIR      set the name of build directory (default is `%{name}-%{version}`)
-T          skip the default unpacking of the first source (used with
            `-a` or `-b`)
-q          operate quietly
```

#### %patch

`%patch [options] [arguments]`

`%patch` is used to apply patches on top of the just unpacked pristine sources.
Historically it supported multiple strange syntaxes and buggy behaviors,
which are no longer maintained.  To apply patch number 1, the following
are recognized:

1. `%patch 1` (since rpm >= 4.18)
2. `%patch -P1` (all rpm versions)
3. `%patch1` (removed since rpm >= 4.20)

For new packages, the positional argument form 1) is preferred. For maximal
compatibility use 2). Both forms can be used to apply several patches at once,
in the order they appear on the line.

It accepts a number of options. With the exception of `-P`, they are merely
passed down to the `patch` command.
```
-b SUF      backup patched files with suffix SUF
-d DIR      change to directory DIR before doing anything else
-E          remove files emptied by patching
-F N        maximum fuzz factor (on context patches)
-p N        strip N leading slashes from paths
-R          assume reversed patch
-o FILE     send output to FILE instead of patching in place 
-z SUF      same as -b
-Z          set mtime and atime from context diff headers using UTC

-P N        apply patch number N, same as passing N as a positional argument
```
 
### %generate_buildrequires (since rpm >= 4.15)

This optional script can be used to determine `BuildRequires`
dynamically. If present it is executed after %prep and can though
access the unpacked and patched sources. The script must print the found build
dependencies to stdout in the same syntax as used after
`BuildRequires:` one dependency per line.

`rpmbuild` will then check if the dependencies are met before
continuing the build. If some dependencies are missing a package with
the `.buildreqs.nosrc.rpm` postfix is created, that - as the name
suggests - contains the found build requires but no sources. It can be
used to install the build requires and restart the build.

On success the found build dependencies are also added to the source
package. As always they depend on the exact circumstance of the build
and may be different when bulding based on other packages or even
another architecture.

### %conf (since rpm >= 4.18)

In %conf, the unpacked sources are configured for building.

Different build- and language ecosystems come with their
own helper macros, but rpm has helpers for autotools based builds such as
itself which typically look like this:

```
%conf
%configure
```

### %build

In %build, the unpacked (and configured) sources are compiled to binaries.

Different build- and language ecosystems come with their
own helper macros, but rpm has helpers for autotools based builds such as
itself which typically look like this:

```
%build
%make_build
```

### %install

In `%install`, the software installation layout is prepared by creating
the necessary directory structure into an initially empty "build root"
directory and copying the just-built software in there to appropriate
places. For many simple packages this is just:

```
%install
%make_install
```

`%install` required for creating packages that contain any files.

### %check

If the packaged software has accomppanying tests, this is where they
should be executed.

### %clean (OBSOLETE)

Packages should place all their temporaries inside their designated
`%builddir`, which rpm will automatically clean up. Needing a package
specific `%clean` section generally suggests flaws in the spec.

## Runtime scriptlets

Runtime scriptlets are executed at the time of install and erase of the
package. By default, scriptlets are executed with `/bin/sh` shell, but
this can be overridden with `-p <path>` as an argument to the scriptlet
for each scriptlet individually. Other supported operations include
[scriptlet expansion](scriptlet_expansion.md).

### Basic scriptlets

 * `%pre`
 * `%post`
 * `%preun`
 * `%postun`
 * `%pretrans`
 * `%posttrans`
 * `%preuntrans`
 * `%postuntrans`
 * `%verify`

### Triggers

 * `%triggerprein`
 * `%triggerin`
 * `%triggerun`
 * `%triggerpostun`

More information is available in [trigger chapter](triggers.md).

### File triggers (since rpm >= 4.13)

 * `%filetriggerin`
 * `%filetriggerun`
 * `%filetriggerpostun`
 * `%transfiletriggerin`
 * `%transfiletriggerun`
 * `%transfiletriggerpostun`

More information is available in [file trigger chapter](file_triggers.md).

## %files section

### Permissions and ownership

By default, packaged files are owned by root:root and permission bits
are taken directly from the on-disk files. Refer to
[users and groups](users_and_groups.md) for dealing with other users
and groups.

Note that rpm only uses information from the local passwd(5) and group(5)
files.

There are two directives to override the default:

#### `%attr(<mode>, <user>, <group>) <file|directory>`

`%attr()` overrides the permissions for a single file. `<mode>` is
an octal number such as you'd pass to `chmod`(1), `<user>` and `<group>`
are user and group names. Any of the three can be specified as `-` to
indicate use of current default value for that parameter.

#### `%defattr(<mode>, <user>, <group>, <dirmode>)`

`%defattr()` sets the default permissions of the following entries in
up to the next `%defattr()` directive or the end of the `%files` section
for that package, whichever comes first.

The first three arguments are the same as for `%attr()` (see above),
`<dirmode>` is the octal default mode for directories.

### Virtual File Attribute(s)

#### %artifact (since rpm >= 4.14.1)

The %artifact attribute can be used to denote files that are more like
side-effects of packaging than actual content the user would be interested
in. Such files can be easily filtered out on queries and also left out
of installations if space is tight.

#### %ghost

A %ghost tag on a file indicates that this file is not to be included
in the package.  It is typically used when the attributes of the file
are important while the contents is not (e.g. a log file).

#### %config

The %config(missingok) indicates that the file need not exist on the
installed machine. The %config(missingok) is frequently used for files
like /etc/rc.d/rc2.d/S55named where the (non-)existence of the symlink
is part of the configuration in %post, and the file may need to be
removed when this package is removed.  This file is not required to
exist at either install or uninstall time.

The %config(noreplace) indicates that the file in the package should
be installed with extension .rpmnew if there is already a modified file
with the same name on the installed machine.

#### %dir

Used to explicitly own the directory itself but not it's contents.

#### %doc

Used to mark and/or install files as documentation. Can be used as a
regular attribute on an absolute path, or in "special" form on a path
relative to the build directory which causes the files to be installed
and packaged as documentation. The special form strips all but the last
path component. Thus `%doc path/to/docfile` installs `docfile` in the
documentation path.

Can also be used to filter out documentation from installations where
space is tight.

#### %license (since rpm >= 4.11)

Used to mark and/or install files as licenses. Same as %doc, but 
cannot be filtered out as licenses must always be present in packages.

#### %missingok (since rpm >= 4.14 in standalone form)

Used to mark file presence optional, ie one whose absence does not
cause --verify to fail.

#### %readme

Obsolete.

#### %verify

The virtual file attribute token %verify tells `-V/--verify' to ignore
certain features on files which may be modified by (say) a postinstall
script so that false problems are not displayed during package verification.
```
	%verify(not size filedigest mtime) %{prefix}/bin/javaswarm
```

Supported modifiers are:
* filedigest (or md5)
* size
* link
* user (or owner)
* group
* mtime
* mode
* rdev
* caps

### Shell Globbing

The usual rules for shell globbing apply (see `glob(7)`), including brace
expansion.  Metacharacters can be escaped by prefixing them with a backslash
(`\`).  Spaces are used to separate file names and must also be escaped.
Enclosing a file name in double quotes (`"`) preserves the literal value of all
characters within the quotes, with the exception of `\` and the percent sign
(`%`).  A `\` or `%` can be escaped with an extra `\` or `%`, respectively.  A
double quote can be escaped with a `\`.

For example:

```
	/opt/are.you|bob\?
	/opt/bob's\*htdocs\*
	/opt/bob's%%htdocs%%
	"/opt/bob's htdocs"
```

If a glob pattern has no matches, it is tried literally (as if all the
metacharacters were escaped).  This is similar to how Bash works with the
`failglob` option unset.

When trying to escape a large number of file names, it is often best to create
a file with a complete list of escaped file names.  This is easiest to do with
a shell script like this:

```
	rm -f filelist.txt
	find %{buildroot} -type f -printf '/%%P\n' |	\
	perl -pe 's/(%)/%$1/g;'				\
	     -pe 's/(["\\])/\\$1/g;'			\
	     -pe 's/(^.*$)/"$1"/g;'			\
	> filelist.txt

	%files -f filelist.txt
```

## %changelog section
