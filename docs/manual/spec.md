---
layout: default
title: rpm.org - Spec file format
---
# Spec file format

## Generic syntax

### Comments

Comments in spec file have # at the start of the line.

```
    # this is a comment
```

Macros are expanded even in comment lines. If this is undesireable, escape
the macro with an extra percent sign (%):

```
    # make unversioned %%__python an error unless explicitly overridden
```

Another option is to use built-in macro %dnl that discards text to next
line without expanding it.

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

## Preamble

### Preamble tags

#### Name

The Name tag contains the proper name of the package. Names must not
include whitespace and may include a hyphen '-' (unlike version and release
tags). Names should not include any numeric operators ('<', '>','=') as
future versions of rpm may need to reserve characters other than '-'.

By default subpackages are named by prepending `\<main package\>-' to
the subpackage name(s). If you wish to change the name of a
subpackage (most commonly this is to change the '-' to '.'), then you
must specify the full name with the -n argument in the %package
definition:

```
	%package -n newname
```

#### Version

Version of the packaged content, typically software.

The version string consists of alphanumeric characters, and can optionally
be segmented with separators `.`, ```_``` and `+`.

Tilde (`~`) can be used to force sorting lower than base (1.1~201601 < 1.1)
Caret (`^`) can be used to force sorting higher than base (1.1^201601 > 1.1)
These are useful for handling pre- and post-release versions, such as
1.0~rc1 and 2.0^a.

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

#### Patch

Used to declare patches applied on top of sources. All patches declared
will be packaged into source rpms.

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

  Also relates to `%pretrans` scriptlet execution.

* `posttrans`

  Denotes the dependency must be present at the end of transaction, ie
  cannot be removed during the transaction. As such, it does not affect
  transaction ordering. A posttrans-dependency is free to be removed
  after the the install-transaction completes.

  Also relates to `%posttrans` scriptlet execution.

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

#### Recommends
#### Suggests
#### Supplements
#### Enhances

#### OrderByRequires

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

Package is not buildable on specific OS'es listed here.

#### ExclusiveOS

Package is only buildable on OS'es listed here.

#### BuildArch (or BuildArchitectures)

Specifies the architecture which the resulting binary package
will run on.  Typically this is a CPU architecture like sparc,
i386. The string 'noarch' is reserved for specifying that the
resulting binary package is platform independent.  Typical platform
independent packages are html, perl, python, java, and ps packages.

As a special case, `BuildArch: noarch` can be used on sub-package
level to allow eg. documentation of otherwise arch-specific package
to be shared across multiple architectures.

#### Prefixes (or Prefix)

Specify prefixes this package may be installed into, used to make
packages relocatable. Very few packages are.

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

### Sub-sections

#### %description

%description is free form text, but there are two things to note.
The first regards reformatting.  Lines that begin with white space
are considered "pre-formatted" and will be left alone.  Adjacent
lines without leading whitespace are considered a single paragraph
and may be subject to formatting by glint or another RPM tool.

### Dependencies

## Build scriptlets

Package build is divided into multiple separate steps, each executed
in a separate shell.

### %prep

%prep prepares the sources for building. This is where sources are
unpacked and possible patches applied, and other similar activies
could be performed.

Typically (%autosetup)[autosetup.md] is used to automatically handle
it all, but for more advanced cases there are lower level `%setup`
and `%patch` builtin-macros available in this slot.

In simple packages `%prep` is often just:
```
%prep
%autosetup
```

### %generate_buildrequires

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

### %build

In %build, the unpacked sources are compiled to binaries.
Different build- and language ecosystems come with their
own helper macros, but rpm has helpers for autotools based builds such as
itself which typically look like this:

```
%build
%configure
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
 * `%verify`

### Triggers

 * `%triggerprein`
 * `%triggerin`
 * `%triggerun`
 * `%triggerpostun`

More information is available in [trigger chapter](triggers.md).

### File triggers

 * `%filetriggerin`
 * `%filetriggerun`
 * `%filetriggerpostun`
 * `%transfiletriggerin`
 * `%transfiletriggerun`
 * `%transfiletriggerpostun`

More information is available in [file trigger chapter](file_triggers.md).

## %files section

### Virtual File Attribute(s)

#### %artifact

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
and packaged as documentation.

Can also be used to filter out documentation from installations where
space is tight.

#### %license

Used to mark and/or install files as licenses. Same as %doc, but 
cannot be filtered out as licenses must always be present in packages.

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

The usual rules for shell globbing apply.  Most special characters can
be escaped by prefixing them with a '\'.  Spaces are used to separate
file names and so must be escaped by enclosing the file name with quotes.
For example:

```
	/opt/are\.you\|bob\?
	/opt/bob\'s\*htdocs\*
	"/opt/bob\'s htdocs"
```

Names containing "%%" will be rpm macro expanded into "%".  When
trying to escape large number of file names, it is often best to
create a file with the complete list of escaped file names.  This is 
easiest to do with a shell script like this:

```
	rm -f filelist.txt 
	find $RPM_BUILD_ROOT/%{_prefix} -type f -print | \
	    sed "s!$RPM_BUILD_ROOT!!" |  perl -pe 's/([?|*.\'"])/\\$1/g' \
		>> $RPM_BUILD_DIR/filelist.txt 

	%files -f filelist.rpm
```

## %changelog section
