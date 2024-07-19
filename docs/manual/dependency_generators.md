---
layout: default
title: rpm.org - RPM Dependency Generator
---
# RPM Dependency Generator

One basic design goal of RPM is to be able to check if a package really is going to work on the system it is being installed. To achive this, RPM does not (only) rely on requiring packages as stated by the packagers, but auto detects executables and libraries that are used. To make this work, RPM also examines the packages for libraries that could be used by other packages and creates "Provides:" for them.

This works reasonably well for binary executables and shared object files (libraries). Wherever there is no automatic generation of `Provides:` and `Requires:`, packagers can resort to add manual `Requires:` to the packages they need, or even add `Provides:` to the packages by hand. Over the years, this has led to huge areas that are not covered by RPM's auto generator. To claim back these areas, a plug-in interface was created to allow packagers/distributions to auto-create dependencies for single domains without tampering with the rpm code.

## File Attributes
To avoid passing every file through every dependency generator, file attributes are used. All files in packages are classified based on the present file attribute rules, and files can have an arbitrary number of attributes. Each file attribute can have dependency generators for all supported types.

A file attribute is represented by a macro file in `%{_fileattrsdir}` (typically `/usr/lib/rpm/fileattrs/`), and must have the `.attr` suffix to be processed. The following file attribute macros are recognized:

```
%__NAME_conflicts
%__NAME_enhances
%__NAME_obsoletes
%__NAME_orderwithrequires
%__NAME_provides
%__NAME_requires
%__NAME_recommends
%__NAME_suggests
%__NAME_supplements

%__NAME_path
%__NAME_magic
%__NAME_flags
%__NAME_exclude_path
%__NAME_exclude_magic
%__NAME_exclude_flags
%__NAME_protocol
```

NAME needs to be replaced by the name choosen for the file attribute and needs to be the same as the file name of the macro file itself (without the `.attr` suffix). While technically all of them are optional, typically at least two of them are present to form a meaningful attribute: `*_path` and/or `*_magic` to match any files at all and at least one generator. All the values are further macro-expanded on use, and additionally, the path and magic related values are interpreted as extended regular expressions.

The path REs are matched against the packaged path, without `%buildroot` - e.g. `/bin/bash`. The magic REs are matched against the result of libmagic (see `man file` and `man magic`), and some of them are also stored in the `FILECLASS` tag (try `rpm -q --fileclass PACKAGENAME` for example). To get compatible results with rpm, use `file -z -e tokens <file>` when determining appropriate file attribute magic REs. Matching is inclusive unless changed by flags: if path, magic or both match, the file is considered to have the attribute in question, unless there is a matching exclude pattern (also inclusive by default) or a flag which prevents the match.


Flags are a comma-separated lists. As of rpm 4.9.1, the supported flags are:
* `exeonly` - require executable bit set
* `magic_and_path` - require both magic and pattern to match

## Generators
A generator is just an executable that reads file name(s) from stdin and writes out dependencies on stdout, one per line. Note that unlike the classifier
path RE, the paths passed to generators have `%buildroot` prepended, as most
generators need to look at the actual file contents on disk.
This way, the generator can be implemented in whatever language is preferred and can use e.g. language specific libraries or tools. Generators get called once for each file with matching attributes. Generators can be declared in the file attributes file by defining the following macros:

```
%__NAME_conflicts
%__NAME_enhances
%__NAME_obsoletes
%__NAME_orderwithrequires
%__NAME_provides
%__NAME_requires
%__NAME_recommends
%__NAME_suggests
%__NAME_supplements
```

Note that generating `OrderWithRequires` will appear only in rpm >= 4.17.

The value is the command line of the generator script/executable and any arguments that should be passed to it. In addition to what is defined in the provides/requires macros, it is possible to pass additional arbitrary switches to generators by defining the following macros:

```
%__NAME_conflicts_opts
%__NAME_enhances_opts
%__NAME_obsoletes_opts
%__NAME_orderwithrequires_opts
%__NAME_provides_opts
%__NAME_requires_opts
%__NAME_recommends_opts
%__NAME_suggests_opts
%__NAME_supplements_opts
```

The `_opts` macros should not be used in file attribute definitions, as they are intended for spec-specific tweaks only. Note that any option is fully generator-specific, and rpm only requires generators to support the stdin-stdout protocol.

## Old Style Dependency Generators
Old style generators, also known as "the external dependency generator", differ from the "internal" one in several ways. One difference that generator developers need to be aware of is that the new generators get called once for each file of a type, but the old generator is passed the entire file list of a package all at once. For compatibility reasons, all generators should accept an arbitrary number of files on stdin. A more profound difference is the data generated: packages built with old-style generators contain less data about the files, such as "color" information which is vital for rpm's functionality on multiarch systems, file type information and per-file dependency tracking. The old-style generators are deprecated and should not be used for new packaging, as this functionality is only kept for backwards compatibility and may get removed in a future release of rpm.

## Writing Dependency Generators
Generally, each type of dependency should be handled in one place. The idea is not that every package ships its own generators, but one central package is taking care. Therefore, generators should go either directly into RPM (talk to us on the rpm-maint list), or be shipped in one central package dealing with the domain. This would be something like the interpreter of the language in question or the package containing a central reqistry or even just the directory where other packages are supposed to put their files into.

Macros and other external data can be passed to generator scripts through the
generator command line, for example to pass source package name and version
to a generator:

```
%__foo_requires 	/some/script %{NAME} %{VERSION}
```

In addition to globally defined macros, the following macros are exported to
generators on sub-package basis (rpm >= 4.15):
- `%name`
- `%epoch`
- `%version`
- `%release`

### Parametric macro generators (rpm >= 4.16)

If the generator macro is declared as a parametric macro, the macro itself
is called for dependency generation, with file as first argument (ie `%1`)
and the macro expansion itself is considered as the generated dependencies,
one per line. rpm's own macro primitives are limited, but by using `%{lua:...}`,
enables writing complicated macro-only generators.
As with traditional generators, the path passed as `%1` has `%buildroot`
prepended.

A trivial example to create provides from path basenames, which would be
enormously faster than the equivalent traditional generator
shelling out to execute a script that calls `basename`:

```
%__foo_provides()	%{basename:%{1}}
```

### Multifile protocol (rpm >= 4.20)

Generators may optionally support an enhanced mode where all matching
files are passed to the generator at once, and the generator is expected
to output the processed filename prepended with `;` before outputing
dependencies of that file. Files with no dependencies may be omitted.
This is several orders of magnitude faster than the traditional behavior
where a generator is launched separately for each and every file.
Enabling the multifile mode is done by setting
`%__NAME_protocol` to `multifile` in the attribute file, eg

```
%__foo_protocol multifile
```

## Using File Attributes in their own Package

Normally file attributes and their dependency generators are shipped in separate packages that need to be installed before the package making use of them can be build.

Since rpm 4.20 the names of file attributes from the package itself can be put into the *_local_file_attrs* macro separated by colons (:). The macros that normally go into the *\*.attr* files still need to be defined (the dependency generators typically pointing to some Source files or some files in the install root).

This mechanism can be used for both file attributes the package ships to be installed but also for file attributes that are used during the own building process only.

For the former packagers need to be aware that a previus version of the package might be installed on the system the package is build on. Thus the Spec file must set all macros used in the past and undefine the ones not longer being used.

## Tweaking Dependency Generators
Technically, all aspects of file attributes and the generator helpers they use can be overridden from spec by (re)defining the related macros, but packagers should generally avoid this, as the attributes and their names are subject to change, depending on rpm version and which packages are present during build. Unwanted dependencies can be filtered with a separate set of macros which are intended primarily for use in spec files:

```
%__requires_exclude
%__requires_exclude_from
%__provides_exclude
%__provides_exclude_from
```

The values of these are macro-expanded and the results interpreted as (extended) regular expressions. The `exclude_from` variants operate on paths, for example to prevent all provides generated from `*.so` files in `%{_libdir}/mypkg/` from being added:

```
%define __provides_exclude_from ^%{_libdir}/mypkg/.*.so$
```

This would prevent any provides from `*.so` files in `%{_libdir}/mypkg/` from being added to packages. The exclude variants operate on generated dependency strings. For example, the following would prevent all typical library requires from being added, regardless of which files they originate from:

```
%define __requires_exclude ^lib.*$
```

Note that within spec files, any backslashes need to be double-escaped to prevent the spec parser from eating them.

## Troubleshooting
rpmbuild and rpmdeps have a hidden `--rpmfcdebug` switch that enables additional output for the dependency generation stage. The output format is currently roughly as follows:

```
<file number> <on-disk path> <color info> <file attribute matches>
    <list of dependencies associated with this file>
```

File attribute matches are the names of the fileattr rules which matched for the file, and that is where rule troubleshooting typically starts: a file with no attributes will not have any dependencies attached.

## Examples
* [File attributes shipped with RPM](https://github.com/rpm-software-management/rpm/tree/master/fileattrs)
* [Scripts shipped with RPM (some of the used by file attributes)](https://github.com/rpm-software-management/rpm/tree/master/scripts)
