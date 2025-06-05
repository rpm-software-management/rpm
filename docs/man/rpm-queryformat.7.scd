RPM-QUERYFORMAT(7)

# NAME
*rpm-queryformat* - RPM query format

# SYNOPSIS

%[_WIDTH_]{_TAG_[:_FORMAT_]}

# DESCRIPTION

The *rpm* query format is a static string modeled after the standard *printf*(3)
format. It uses *rpm* header tags enclosed in curly braces in place of
conversion specifiers, and allows you to specify what package information should
be printed in query operations and how it should be formatted.

Each _TAG_ in the string may specify an output _FORMAT_ to further customize the
way it is displayed. Special characters such as newlines or tabs (with the
exception of *\\0*), as well as the *printf*(3) field _WIDTH_, are also
supported.

The string is accepted as an argument to the *--queryformat* option in *rpm*(8)
and in runtime scriptlets using the *-q* option in *rpm-spec*(5) files.

## Tags

Every *rpm* package contains a set of meta-data records describing its
properties, such as the name, version or file list. Each record is either a
plain string or number, or an array of either type. These records are stored
inside a package, in a section called the _header_, and are identified by unique
names called _tags_. The individual records are also referred to as _tag data_.

You can use these tags in the query format to display the individual properties
of a package. To obtain a list of all the tags supported by the version of *rpm*
installed on your system, run the following command:

```
rpm --querytags
```

This will print a list of tags, one per line, for example:

```
BUILDHOST
BUILDTIME
DESCRIPTION
EPOCH
INSTALLTIME
NAME
RELEASE
SIZE
SUMMARY
VERSION
```

Each tag is case-insensitive and also has an implicit variant with the *RPMTAG_*
prefix, such as *RPMTAG_NAME*, which points to the same record. Both variants
can be used in the query format interchangeably.

## Formats

Each tag is printed in a specific output _format_. When no format is specified,
the default *:string* format is used which displays a bare-bones representation
of the tag data. You can specify a different format for a tag by appending a
colon followed by the format name to the tag name, for example:

```
%{INSTALLTIME:date}
```

This is useful when displaying a tag that's not stored in human-readable form,
such as a size or date, or when you want to perform a formatting operation on
the tag data before it's displayed, such as wrapping it in JSON.

The following formats are available:

*:armor*
	Wrap a public key in ASCII armor.

*:arraysize*
	Display number of elements in array tags.

*:base64*
	Encode binary data using base64.

*:date*
	Use *strftime*(3) "%c" format.

*:day*
	Use *strftime*(3) "%a %b %d %Y" format.

*:depflags*
	Format dependency comparison operator.

*:deptype*
	Format dependency type.

*:expand*
	Perform macro expansion.

*:fflags*
	Format file flags.

*:fstate*
	Format file state.

*:fstatus*
	Format file verify status.

*:hashalgo*
	Display hash algorithm name.

*:hex*
	Format in hexadecimal.

*:octal*
	Format in octal.

*:humaniec*
	Human readable number (in IEC 80000). The suffix K = 1024, M =
	1048576, ...

*:humansi*
	Human readable number (in SI). The suffix K = 1000, M = 1000000, ...

*:json*
	Wrap data in JSON.

*:perms*
	Format file permissions.

*:pgpsig*
	Display signature fingerprint and time.

*:shescape*
	Escape single quotes for use in a script.

*:string*
	Display string format. (default)

*:tagname*
	Display tag name.

*:tagnum*
	Display tag number.

*:triggertype*
	Display trigger suffix.

*:vflags*
	File verification flags.

*:xml*
	Wrap data in simple XML markup.

## Field width

Similarly to *printf*(3), you can specify the minimum width of a given tag when
printed, by placing a number in between the *%* and *{* characters. Optionally,
prefix the number with the *-* sign to left justify it.

## Arrays

*rpm* uses many parallel arrays internally. For example, file sizes and file
names are kept as an array of numbers and an array of strings, respectively,
with the first element in the size array corresponding to the first element in
the name array. 

To iterate over a set of parallel arrays, enclose the format to be used to print
each item in the array within square brackets. For example, to print all of the
files and their sizes in the *slang-devel* package, with one file per line, use
this command:

```
rpm -q --queryformat "[%-50{FILENAMES} %10{FILESIZES}\\n]" slang-devel
```

Note that since the trailing newline is inside the square brackets, one newline
is printed for each filename.

A popular query format to try to construct is one that prints the name of a
package and the name of a file it contains on one line, repeated for every file
in the package. This query can be very useful for passing information to any
program that's line oriented (such as *grep*(1) or *awk*(1)). If you try the
obvious:

```
rpm -q --queryformat "[%{NAME} %{FILENAMES}\\n]" cdp
```

you will see *rpm* complain about an "array iterator used with different sized
arrays". Internally, all items in *rpm* are actually arrays, so the *NAME* is a
string array containing one element. When you tell *rpm* to iterate over the
*NAME* and *FILENAMES* elements, *rpm* notices the two tags have different
numbers of elements and complains.

To make this work properly, you need to tell *rpm* to always print the first
item in the *NAME* element. You can do this by placing the *=* sign before the
tag name, like this:

```
rpm -q --queryformat "[%{=NAME} %{FILENAMES}\\n]" cdp
```

which will give you the expected output:

```
cdp /usr/bin/cdp
cdp /usr/bin/cdplay
cdp /usr/man/man1/cdp.1
```

## Expressions

Simple conditionals may be evaluated through query _expressions_ in the form of

```
%|EXPR|
```

where _EXPR_ is an expression.

The only type of expression currently supported is a C-like ternary conditional
for simple if/then/else branching, and has the following form:

```
%|TAG?{QF-DEFINED}:{QF-UNDEFINED}|
```

where _QF-DEFINED_ and _QF-UNDEFINED_ are query formats that should be expanded
if _TAG_ is defined and undefined, respectively. Thus, query expressions can be
nested.

# EXAMPLES
## Example 1. Query commands

*rpm -qa --queryformat "%-30{NAME} %{SIZE:humaniec}\\n"*
	Print a table of all installed package names and their human-readable
	sizes.

*rpm -q --queryformat "%{NAME} %{INSTALLTIME:date}\\n" fileutils*
	Print the *fileutils* package name followed by its installation date.

*rpm -q --queryformat "[%{FILEMODES:perms} %{FILENAMES}\\n]" rpm*
	Print all filenames owned by the *rpm* package, one per line, with each
	name preceded by a file permissions string similar to the long listing
	format of *ls*(1).

*rpm -q --queryformat "[%{REQUIRENAME} %{REQUIREFLAGS:depflags}
%{REQUIREVERSION}\\n]" vlock*
	Print all capabilities on which the *vlock* package depends in the form
	of version comparisons, one per line, with the capability name and the
	required version as the left and right operand, respectively.

*rpm -qa --queryformat "%-30{NAME} %|PREINPROG?{ %{PREINPROG}}:{ no}|\\n"*
	Print a table of all installed package names in the left column and the
	program names used for their *prein* scriptlets (such as the default
	*/bin/sh*) in the right column, with the latter being *no* if the given
	package has no *prein* scriptlet.

## Example 2. Command aliases

_/usr/lib/rpm/rpmpopt-VERSION_
	A *popt*(3) alias file used by *rpm*(8) itself to implement various
	shorthand options (such as *--scripts*) with the help of query formats.
	Replace _VERSION_ with the version of *rpm* installed on your system.

## Example 3. Runtime scriptlets

Print the exact file list of a package after its installation:

```
%post -q
for f in [%%{instfilenames} ]; do
    echo $f
done
```

Note that the trailing space inside the square brackets is required in order for
the query format to be expanded to separate words that the *for* statement can
loop over.

See the scriptlet expansion documentation (in *SEE ALSO*) and *rpm-spec*(5) for
more information on runtime scriptlets.

# SEE ALSO

*rpm*(8), *rpm-common*(8), *rpm-config*(5), *rpm-spec*(5), *popt*(3)

*https://rpm.org/docs/latest/manual/scriptlet_expansion*
