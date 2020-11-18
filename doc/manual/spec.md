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

Name

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

### Summary and description

The Summary: tag should be use to give a short (50 char or so) summary
of the package.  

%description is free form text, but there are two things to note.
The first regards reformatting.  Lines that begin with white space
are considered "pre-formatted" and will be left alone.  Adjacent
lines without leading whitespace are considered a single paragraph
and may be subject to formatting by glint or another RPM tool.

### URL: and Packager: Tags

"URL:" is a place to put a URL for more information and/or documentation
on the software contained in the package.

Packager: tag is meant to contain the name and email
address of the person who "maintains" the RPM package (which may be
different from the person who actually maintains the program the
package contains).

### BuildArchitectures: Tag

This tag specifies the architecture which the resulting binary package
will run on.  Typically this is a CPU architecture like sparc,
i386. The string 'noarch' is reserved for specifying that the
resulting binary package is platform independent.  Typical platform
independent packages are html, perl, python, java, and ps packages.

### Dependencies

#### Fine Adjustment of Automatic Dependencies

Rpm currently supports separate "Autoreq:" and "Autoprov:" tags in a
spec file to independently control the running of find-requires and
find-provides. A common problem occurs when packaging a large third
party binary which has interfaces to other third party libraries you
do not own.  RPM will require all the third party libraries be
installed on the target machine even though their intended use was
optional. To rectify the situation you may turn off requirements when
building the package by putting

```
	Autoreq: 0 
```

in your spec file. Any and all requirements should be added manually using the

```
	Requires: depend1, ..., dependN
```

in this case.

Similarly there is an Autoprov tag to turn off the automatic provision
generation and a Autoreqprov to turn off both the automatic provides and
the automatic requires generation.

### NoSource: Tag

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

### BuildRequires: Tag

Build dependencies are identical to install dependencies except:

```
  1) they are prefixed with build (e.g. BuildRequires: rather than Requires:)
  2) they are resolved before building rather than before installing.
```

So, if you were to write a specfile for a package that requires egcs to build,
you would add
```
	BuildRequires: egcs
```
to your spec file.

If your package was like dump and could not be built w/o a specific version of
the libraries to access an ext2 file system, you could express this as
```
	BuildRequires: e2fsprofs-devel = 1.17-1
```

Finally, if your package used C++ and could not be built with gcc-2.7.2.1, you
can express this as
```
	BuildConflicts: gcc <= 2.7.2.1
```

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
All files installed in the buildroot must be packaged.

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

A %ghost tag on a file indicates that this file is not to be included
in the package.  It is typically used when the attributes of the file
are important while the contents is not (e.g. a log file).

The %config(missingok) indicates that the file need not exist on the
installed machine. The %config(missingok) is frequently used for files
like /etc/rc.d/rc2.d/S55named where the (non-)existence of the symlink
is part of the configuration in %post, and the file may need to be
removed when this package is removed.  This file is not required to
exist at either install or uninstall time.

The %config(noreplace) indicates that the file in the package should
be installed with extension .rpmnew if there is already a modified file
with the same name on the installed machine.

The virtual file attribute token %verify tells `-V/--verify' to ignore
certain features on files which may be modified by (say) a postinstall
script so that false problems are not displayed during package verification.
```
	%verify(not size filedigest mtime) %{prefix}/bin/javaswarm
```

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

With sub-packages it's common to end up with all but that one file in
a single package and the one in a sub-package. This can be handled
with `%exclude`, for example:

```
	%files
	%{_bindir}/*
	%exclude %{_bindir}/that-one-file

	%files sub
	%{_bindir}/that-one-file
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
