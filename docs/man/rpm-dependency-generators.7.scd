RPM-DEPENDENCY-GENERATORS(7)

# NAME
*rpm-dependency-generators* - RPM file attribute and dependency generator
interface

# SYNOPSIS

## File attributes
*%\_\_*_NAME_*\_path* _PATH_RE_++
*%\_\_*_NAME_*\_magic* _MAGIC_RE_++
*%\_\_*_NAME_*\_mime* _MIME_RE_

*%\_\_*_NAME_*\_exclude_path* _PATH_RE_++
*%\_\_*_NAME_*\_exclude_magic* _MAGIC_RE_++
*%\_\_*_NAME_*\_exclude_mime* _MIME_RE_

*%\_\_*_NAME_*\_flags* _FLAG_[*,*_FLAG_ ...]++
*%\_\_*_NAME_*\_exclude_flags* _FLAG_[*,*_FLAG_ ...]

## Generators
*%\_\_*_NAME_*\_*​_TYPE_ _COMMAND_++
*%\_\_*_NAME_*\_*​_TYPE_*()* _BODY_

*%\_\_*_NAME_*\_protocol* {*singlefile*|*multifile*}

## Per-package tunables
*%\_local_file_attrs* _NAME_[*:*_NAME_ ...]++
*%\_\_*_TYPE_*\_exclude* _DEPSTRING_RE_++
*%\_\_*_TYPE_*\_exclude_from* _PATH_RE_++
*%\_\_*_NAME_*\_*​_TYPE_*\_opts* _OPTS_

## Notes
_TYPE_ can be one of *conflicts*, *enhances*, *obsoletes*, *orderwithrequires*,
*provides*, *recommends*, *requires*, *suggests* and *supplements*. _FLAG_ can
be one of *exeonly* and *magic_and_path*.

# DESCRIPTION

RPM uses a pluggable interface coupled with a file classifier to add dependency
information to packages at build time. This system relies on specialized
executables, called _generators_, that do the heavy-lifting of extracting
dependencies from files in their domain, called a _file attribute_. It
complements the manual tags, such as *Requires* or *Provides*, in *rpm-spec*(5)
files.

# FILE ATTRIBUTES

A file attribute is a logical label that RPM gives to one or more files in the
buildroot, based on a set of _matching rules_ and their _tunables_, defined as
macros. It can represent a single file type, such as ELF, or a broader domain,
such as all files installed into a specific location or written in a specific
programming language.

These attributes are distinct from the file attributes in the filesystem and are
only kept in memory during package builds. A single file may have an arbitrary
number of attributes, and a single attribute may be assigned to an arbitrary
number of files.

To introduce a file attribute called _NAME_ for use during package builds, drop
an *rpm-macrofile*(5) named _NAME.attr_ into the *%{\_fileattrsdir}* directory.
The file should define at least one of the matching rules described below.

## Matching rules
*%\_\_*_NAME_*\_path* _PATH_RE_
	Match *regex*(7) string _PATH_RE_, macro-expanded on use, against
	filenames without the *%{buildroot}* prefix (for example, _/bin/bash_).

*%\_\_*_NAME_*\_magic* _MAGIC_RE_
	Match *regex*(7) string _MAGIC_RE_, macro-expanded on use, against file
	*magic*(5).

	To determine a compatible _MAGIC_RE_, use the command:
	```
	file -z -e tokens /some/file
	```

*%\_\_*_NAME_*\_mime* _MIME_RE_
	Match *regex*(7) string _MIME_RE_, macro-expanded on use, against MIME
	file types.

	To determine a compatible _MIME_RE_, use the command:
	```
	file --mime /some/file
	```

*%\_\_*_NAME_*\_exclude_path* _PATH_RE_++
*%\_\_*_NAME_*\_exclude_magic* _MAGIC_RE_++
*%\_\_*_NAME_*\_exclude_mime* _MIME_RE_
	Filter out files matching the given *regex*(7). These rules are the
	exclusion counterparts of the above inclusion rules.

Matching is inclusive unless changed by flags (see below): If the path, magic or
both match, the file is said to _have_ the attribute in question, unless there
is a matching exclusion rule (also inclusive by default) or a flag that prevents
the match. If both the magic and mime rules are defined, the magic rule is
ignored and a warning is printed.

## Tunables
*%\_\_*_NAME_*\_flags* _FLAGS_++
*%\_\_*_NAME_*\_exclude_flags* _FLAGS_
	Tweak the inclusion and exclusion rules, respectively.

	_FLAGS_ is a comma-separated list of the following possible values:
	- *exeonly*: require executable bit set
	- *magic_and_path*: require both magic and path to match

# GENERATORS

A generator is an executable that reads filenames from stdin and writes
dependencies of a single _type_ to stdout, according to a _protocol_. It is
written for a specific file attribute and defined as a macro in the respective
_NAME.attr_ file. Zero or more generators, one for each supported type, can be
defined for a single file attribute.

## Generator macros
*%\_\_*_NAME_*\_conflicts* _COMMAND_++
*%\_\_*_NAME_*\_enhances* _COMMAND_++
*%\_\_*_NAME_*\_obsoletes* _COMMAND_++
*%\_\_*_NAME_*\_orderwithrequires* _COMMAND_ (Added: 4.17.0)++
*%\_\_*_NAME_*\_provides* _COMMAND_++
*%\_\_*_NAME_*\_recommends* _COMMAND_++
*%\_\_*_NAME_*\_requires* _COMMAND_++
*%\_\_*_NAME_*\_suggests* _COMMAND_++
*%\_\_*_NAME_*\_supplements* _COMMAND_
	Generate dependencies of the respective type. Executes _COMMAND_ (a
	program name and any arguments), passing it one or more filenames that
	have the file attribute _NAME_ on stdin, and reads dependency strings on
	its stdout. See *Protocols* for details on the input and output format.

## Protocols
*%\_\_*_NAME_*\_protocol* _PROTOCOL_
	Use _PROTOCOL_ when running dependency generators for the file attribute
	_NAME_. Determines the input and output format for the generators to
	implement.

	_PROTOCOL_ can be one of the following:

	*singlefile*
		- stdin: one matching filename at a time
		- stdout: dependency strings, one per line

	*multifile*
		- stdin: all matching filenames at once, one per line
		- stdout: dependency strings, one per line, with the original
		  filename, prepended with *;* (semicolon), printed on its own
		  line before the dependencies for that file (Added: 4.20.0)

	If this macro is not defined, the *singlefile* protocol will be used.
	For newly written generators, the *multifile* protocol is recommended
	since it's more performant.

Unlike the _PATH_RE_ in file attributes, generators receive filenames with the
*%{buildroot}* prefix so that they can access the actual file contents on disk.

Generators must always consume all of stdin. For backwards compatibility,
generators should not make any assumptions about the number of files passed,
regardless of the protocol used.

## Exported macros

In addition to globally defined macros, the following macros are automatically
exported to generators, on a subpackage basis:

- *%{name}*
- *%{epoch}*
- *%{version}*
- *%{release}*

Added: 4.15.0

## Parametric generators

If a generator is declared as a parametric macro, the macro itself is called for
dependency generation, with the filename as its first argument (*%1*). The macro
expansion itself is considered as the generated dependencies, one per line.

The following trivial example creates provides from path basenames, which is
enormously faster than the equivalent traditional generator shelling out to
execute a script that calls *basename*(1):

```
%__foo_provides()	%{basename:%{1}}
```

While RPM's own macro primitives are limited, using *%{lua}* enables the writing
of complicated, macro-only generators.

See *PARAMETRIC MACROS* and *Lua expansion* in *rpm-macros*(7) for details.

Added: 4.16.0

## External generators
*%\_\_find_*​_TYPE_ _COMMAND_
	Generate dependencies of the respective _TYPE_. Executes _COMMAND_ (a
	program name and any arguments), passing it the entire file list of the
	package at once, and reads dependency strings on its stdout. See
	*Generator macros* for the possible _TYPE_ values.

This is a legacy, deprecated interface that's only available for v4 packages for
backwards compatibility, and should not be used. Packages built this way contain
less data about their files, such as no "color" information (which is vital for
RPM's functionality on multiarch systems), file type information or per-file
dependency tracking.

To enable this interface regardless, define the macro:
```
%_use_internal_dependency_generator 0
```

# PER-PACKAGE TUNABLES

The following macros can be used on a per-package basis, either by defining them
directly in the *rpm-spec*(5) file or by passing them to *rpmbuild*(1) via the
*--define* option:

*%\_local_file_attrs* _NAMES_
	Register file attribute _NAMES_ (separated by colons) for the duration
	of the build. The respective file attribute and generator macros must be
	defined separately.

	This is useful for running generators shipped by the package itself.

	Added: 4.20.0

*%\_\_*_TYPE_*\_exclude* _DEPSTRING_RE_
	Filter out unwanted dependencies of _TYPE_ by matching *regex*(7) string
	_DEPSTRING_RE_, macro-expanded on use, against dependency strings. See
	*Generator macros* for the possible _TYPE_ values.

	Example:
	```
	%define __requires_exclude ^lib.*$
	```

*%\_\_*_TYPE_*\_exclude_from* _PATH_RE_
	Filter out unwanted dependencies of _TYPE_ by matching *regex*(7) string
	_PATH_RE_, macro-expanded on use, against filenames without the
	*%{buildroot}* prefix (for example, _/bin/bash_). See *Generator macros*
	for the possible _TYPE_ values.

	Example:
	```
	%define __provides_exclude_from ^%{_libdir}/mypkg/.*.so$
	```

*%\_\_*_NAME_*\_*​_TYPE_*\_opts* _OPTS_
	Pass additional command line switches to the _TYPE_ generator. See
	*Generator macros* for the possible _TYPE_ values.

	Note that this macro should NOT be used when writing a file attribute
	file. Instead, just pass the required arguments directly on the
	generator's command line when defining it.

# TROUBLESHOOTING

## Displaying debug information

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

## Global excludes

Global variants of the per-package exclusion macros may also be defined on the
system:

*%\_\_global_*​_TYPE_*\_exclude* _DEPSTRING_RE_++
*%\_\_global_*​_TYPE_*\_exclude_from* _PATH_RE_

These apply to all package builds and thus may affect the output of a particular
generator in unexpected ways. Make sure to inspect the contents of these macros
if that's the case.

# EXAMPLES
## Example 1. A trivial multifile generator

Suppose there's a _%{\_fileattrsdir}/frob.attr_ file with the following
contents:

```
%__frob_provides %{_rpmconfigdir}/frobnize --provides
%__frob_requires %{_rpmconfigdir}/frobnize --requires
%__frob_protocol multifile
%__frob_path \\.frob$
%__frob_magic ^.* a frob file .*$
```

This file defines a file attribute called *frob* that has two generators, both
of which use a _frobnize_ script that implements the *multifile* protocol. The
file attribute matches all files with the _.frob_ suffix as well as those that
contain the "a frob file" magic string.

The _%{\_rpmconfigdir}/frobnize_ script has the following contents:

```
#!/bin/bash
# Frob dependency extractor

# Type of dependency strings to print
mode=$1

# Process every file on stdin
while read -r file; do
    # Print currently processed file as per multifile protocol
    echo ";$file"

    # Get basename and first line
    name=$(basename $file | cut -f1 -d.)
    line=$(head -n1 $file)

    # Print dependency strings
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
done
```

Now, suppose there's a _frob.spec_ file that installs the file _hello.frob_ with
the contents "world" to some location on the filesystem. When the spec is built,
the resulting package will have the following provides and requires:

```
$ rpmbuild -bb /path/to/frob.spec
...
$ rpm -qp --provides --requires /path/to/frob.rpm
...
frob(hello)
frob(world)
bard(hello)
bard(world)
...
```

If multiple files with the _.frob_ suffix or "a frob file" magic were present in
the package, the _frobnize_ script would only be executed once, with all the
matching filenames passed at once.

Note that if the generator decided to *break* from the *while* loop, it would
have to discard any remaining input prior to exiting, as per protocol, which
could be done with the following command placed after the loop:

```
cat > /dev/null
```

## Example 2. Spec-local generators

This spec file snippet introduces two spec-local file attributes, *frob* and
*bard*, that match any and all files, and their generators that use a _frobnize_
and _bardize.sh_ script shipped by the package itself and only included with the
sources, respectively:

```
...
Source1: bardize.sh
...
%define _local_file_attrs frob:bard
%define __frob_provides %{buildroot}/%{_rpmconfigdir}/frobnize
%define __frob_path .*
%define __bard_provides %{SOURCE1}
%define __bard_path .*
...
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
