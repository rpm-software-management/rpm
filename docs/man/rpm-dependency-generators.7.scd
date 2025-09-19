RPM-DEPENDENCY-GENERATORS(7)

# NAME
*rpm-dependency-generators* - RPM file attribute and dependency generator
interface

# SYNOPSIS

## Generators
*%\_\_*_NAME_*\_conflicts*[*()*] _BODY_++
*%\_\_*_NAME_*\_enhances*[*()*] _BODY_++
*%\_\_*_NAME_*\_obsoletes*[*()*] _BODY_++
*%\_\_*_NAME_*\_orderwithrequires*[*()*] _BODY_++
*%\_\_*_NAME_*\_provides*[*()*] _BODY_++
*%\_\_*_NAME_*\_recommends*[*()*] _BODY_++
*%\_\_*_NAME_*\_requires*[*()*] _BODY_++
*%\_\_*_NAME_*\_suggests*[*()*] _BODY_++
*%\_\_*_NAME_*\_supplements*[*()*] _BODY_

## Matching rules
*%\_\_*_NAME_*\_path* _REGEX_++
*%\_\_*_NAME_*\_magic* _REGEX_++
*%\_\_*_NAME_*\_exclude_path* _REGEX_++
*%\_\_*_NAME_*\_exclude_magic* _REGEX_

## Tunables
*%\_\_*_NAME_*\_conflicts_opts* _BODY_++
*%\_\_*_NAME_*\_enhances_opts* _BODY_++
*%\_\_*_NAME_*\_obsoletes_opts* _BODY_++
*%\_\_*_NAME_*\_orderwithrequires_opts* _BODY_++
*%\_\_*_NAME_*\_provides_opts* _BODY_++
*%\_\_*_NAME_*\_recommends_opts* _BODY_++
*%\_\_*_NAME_*\_requires_opts* _BODY_++
*%\_\_*_NAME_*\_suggests_opts* _BODY_++
*%\_\_*_NAME_*\_supplements_opts* _BODY_

*%\_\_requires_exclude* _REGEX_++
*%\_\_requires_exclude_from* _REGEX_++
*%\_\_provides_exclude* _REGEX_++
*%\_\_provides_exclude_from* _REGEX_

# DESCRIPTION

RPM uses a pluggable interface coupled with a file classifier to add dependency
information to packages at build time. This system relies on specialized, domain
specific executables, called _generators_, that do the heavy-lifting of figuring
out the dependencies from the files present in the buildroot. It complements the
manual tags, such as *Requires* or *Provides*, in *rpm-spec*(5) files.

Generators are paired with logical labels, called _file attributes_, assigned to
the packaged files by the file classifier. This ensures that every file is only
processed by the applicable generator(s), as opposed to all generators, which
simplifies their implementation and significantly improves the overall build
performance.

# FILE ATTRIBUTES

A file attribute is a logical label that RPM gives to one or more files in the
buildroot, based on a set of matching _rules_. It can represent a single file
type, such as ELF, or a broader domain, such as all source files written in a
specific programming language.

These attributes are distinct from the file attributes in the filesystem and are
only kept in memory during package builds. A single file may have an arbitrary
number of attributes, and a single attribute may be assigned to an arbitrary
number of files.

A valid file attribute called _NAME_ is introduced with an *rpm-macrofile*(5),
_NAME.attr_, dropped into the *%{\_fileattrsdir}* directory, that defines at
least one of the following macros:

```
%__NAME_path
%__NAME_magic
%__NAME_exclude_path
%__NAME_exclude_magic
```

The values of these macros represent the inclusion and exclusion rules. They are
macro-expanded on use and then interpreted as extended regular expressions (RE).
Matching is inclusive unless changed by _flags_ (see below): If the path, magic
or both match, the file is said to _have_ the attribute in question, unless
there is a matching exclusion rule (also inclusive by default) or a flag that
prevents the match.

The path REs are matched against the packaged path, that is, without the
*%buildroot* prefix (for example, _/bin/bash_). The magic REs are matched
against the result of *magic*(5) classification and possibly the *FILECLASS* tag
stored in the package (see *--fileclass* in *rpm*(8) for details).

Flags are comma-separated lists specified with the following macros:

```
%__NAME_flags
%__NAME_exclude_flags
```

Currently, the following flags are supported (Added: 4.9.1):
- *exeonly*: require executable bit set
- *magic_and_path*: require both magic and path to match

# GENERATORS

A generator is an executable that reads filename(s) from stdin and writes
dependencies of a single _type_ to stdout, one per line. Unlike the path REs,
generators receive filenames with the *%buildroot* prefix since they typically
need to look at the actual file contents on disk. This simple protocol allows
for the generators to be implemented in any preferred language and enables them
to use language specific libraries or tools.

Generators are written _for_ file attributes. Thus, a valid generator for a file
attribute _NAME_ is introduced with any of the following macros (one for every
supported type) defined in the respective _NAME.attr_ file:

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

The value of each macro is a command line (the program name and any arguments)
that should be executed for every file having the _NAME_ attribute.

Macros can also be passed to generators through the command line. For example,
to pass the package name and version to the requires generator *foo*, define the
macro:

```
%__foo_requires 	/some/script %{NAME} %{VERSION}
```

In addition to the globally defined macros, the following macros are exported to
generators on a sub-package basis (Added: 4.15.0):

```
%{NAME}
%{EPOCH}
%{VERSION}
%{RELEASE}
```

Note that the *OrderWithRequires* tag will only appear in RPM >= 4.17.0.

## Parametric macro generators

If the generator macro is declared as a parametric macro (see *PARAMETRIC
MACROS* in *rpm-macros*(7) for details), the macro itself is called for
dependency generation, with the filename as its first argument (*%1*), and the
macro expansion itself is considered as the generated dependencies, one per
line. As with traditional generators, the path passed as *%1* has *%buildroot*
prepended.

While the RPM macro primitives are limited, using *%{lua:...}* enables the
writing of full-featured, macro-only generators in the Lua language.

The following trivial example creates provides from path basenames, which is
enormously faster than the equivalent traditional generator shelling out to
execute a script that calls *basename*(1):

```
%__foo_provides()	%{basename:%{1}}
```

Added: 4.16.0

## Multifile protocol

Generators may optionally support an enhanced mode where all matching files are
passed to the generator at once, and the generator is expected to print the
processed filename prepended with *;* before printing the dependencies of that
file. Files with no dependencies may be omitted.

This is several orders of magnitude faster than the traditional behavior where a
generator is launched separately for each and every file with the matching file
attribute. To enable multifile mode for a generator, define the following macro:

```
%__NAME_protocol multifile
```

Added: 4.20.0

# PACKAGE-LEVEL DEFINITIONS AND TWEAKS

Sometimes, it may be necessary to tweak an installed generator for a specific
package, or to use one that's shipped by the package itself also during its own
build process. This can be done directly in an *rpm-spec*(5) file or by running
*rpmbuild*(1) with the *--define* option(s). The following subsections apply to
both methods.

## File attributes and generators

To introduce new file attributes and generators for the duration of the build,
define them as normal and register them with the following macro (_NAMES_ is a
colon-separated list of file attribute names):

```
%_local_file_attrs NAMES
```

It may be handy to use an executable in the *%{buildroot}* or a source file with
these generators. See *EXAMPLES* for a concrete example.

Added: 4.20.0

## Excludes

Unwanted dependencies can be filtered out with the following set of macros:

```
%__requires_exclude
%__requires_exclude_from
%__provides_exclude
%__provides_exclude_from
```

The values are macro-expanded on use and then interpreted as extended regular
expressions.

The *exclude_from* variants operate on paths. For example, the following macro
will prevent all provides generated from _\*.so_ files in *%{\_libdir}/mypkg/*
from being added:

```
%define __provides_exclude_from ^%{_libdir}/mypkg/.*.so$
```

The *exclude* variants operate on generated dependency strings. For example, the
following macro will prevent all typical library requires from being added,
regardless of which files they originate from:

```
%define __requires_exclude ^lib.*$
```

Note that, within spec files, any backslashes need to be double-escaped in order
to prevent the spec parser from consuming them.

## Additional switches

To pass additional command line switches to generators, define the following
macros:

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

Note that these macros should not be used in a _NAME.attr_ file. Instead, just
pass the required arguments directly on the generator's command line when
defining it.

# COMPATIBILITY NOTES

If compatibility with the RPM 3.x series is desired for a generator, it should
be written in such a way that it accepts an arbitrary number of filenames on
stdin. This is not to be confused with the *Multifile protocol* which was only
added in the RPM 4.x series, however.

This will allow the generator to be used as an old-style, "external" generator,
via the *%\_\_find_requires* and *%\_\_find_provides* macros, which can be
enabled in modern RPM versions with the *%\_use_internal_dependency_generator*
macro (see *rpmbuild-config*(5) for details). However, note that this
functionality has been deprecated in RPM 4.x and therefore should not be used.

# TROUBLESHOOTING

The *rpmbuild*(1) and *rpmdeps*(1) tools have a hidden *--rpmfcdebug* switch
that enables additional output for the dependency generation stage. The output
format is currently roughly as follows:

```
<file number> <on-disk path> <color info> <file attribute matches>
    <list of dependencies associated with this file>
```

File attribute matches are the names of the rules that matched for the file, and
that is where rule troubleshooting typically starts: a file with no attributes
will not have any dependencies attached.

# EXAMPLES
## Example 1. A trivial file attribute and two generators

Assume the following file named _frob.attr_ exists in the *%{\_fileattrsdir}*
directory:

```
%__frob_provides %{_rpmconfigdir}/frobnize --provides
%__frob_requires %{_rpmconfigdir}/frobnize --requires
%__frob_path \\.frob$
%__frob_magic ^.* a frob file .*$
```

This file defines a file attribute named *frob* that matches any files with the
_.frob_ suffix or a magic string containing "a frob file". It also defines two
generators for that attribute that use the script _frobnize_ installed in the
*%{\_rpmconfigdir}* directory, with the following contents:

```
#!/bin/bash
# Frobnize script

mode=$1
file=$(head -n1)
name=$(basename $file | cut -f1 -d.)
line=$(head -n1 $file)

case $mode in
    --provides)
        echo "frob($name)"
        echo "frob($line)"
        ;;
    --requires)
        echo "bard($name)"
        echo "bard($line)"
        ;;
esac
```

This script takes an absolute path from stdin, analyzes the respective file on
disk (reads the first line from it) and prints a list of dependency strings for
provides or requires (depending on the switch used), one per line.

If a package _frob.rpm_ that ships a file _hello.frob_ containing the text
"world" at some location in the filesystem is then built with *rpmbuild*(1), it
will have the following provides and requires:

```
$ rpm -qp --provides --requires frob.rpm
[...]
frob(hello)
frob(world)
bard(hello)
bard(world)
[...]
```

## Example 2. Package-level file attributes and generators

Define two spec-local file attributes named *frob* and *bard* that match any and
all files, each with a provides generator that uses the _frobnize_ executable
shipped with the package and the _bardize.sh_ script included with the sources
(but not installed), respectively:

```
[...]
Source1: bardize.sh
%define _local_file_attrs frob:bard
%define __frob_provides %{buildroot}/%{_rpmconfigdir}/frobnize
%define __frob_path .*
%define __bard_provides %{SOURCE1}
%define __bard_path .*
[...]
```

## Example 3. Bundled file attributes and generators

_%{\_fileattrsdir}/\*.attr_++
_%{\_rpmconfigdir}/\*.req_++
_%{\_rpmconfigdir}/\*.prov_
	A small, curated set of file attributes and their generators bundled
	with RPM itself. These cover the core domains on a UNIX/Linux system,
	such as ELF binaries, executable scripts or *sysusers.d*(5) files.

# SEE ALSO

*rpm*(8) *rpm-common*(8) *rpmbuild*(1) *rpmbuild-config*(5) *rpmdeps*(1)
*rpm-spec*(5) *file*(1) *magic*(5)
