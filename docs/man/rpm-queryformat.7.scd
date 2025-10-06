RPM-QUERYFORMAT(7)

# NAME
*rpm-queryformat* - RPM query mini-language

# SYNOPSIS
## Query format
\[_PLACEHOLDER_|_ITERATOR_|_EXPRESSION_|_TEXT_ ...]

## Placeholder
*%*[_WIDTH_]*{*_TAG_[*:*_FORMAT_]*}*

## Iterator
*[*_QUERYFORMAT_ ...*]*

Note: Square brackets are literal and _QUERYFORMAT_ must contain an array
placeholder.

## Expression
*%|*_TAG_*?{*_PRESENT_*}:{*_ABSENT_*}|*

Note: Vertical bars are literal, _PRESENT_ and _ABSENT_ are query formats.

# DESCRIPTION

RPM supports a simple query language for extracting header data to plain text.
Its syntax resembles that of *printf*(3) where header tags enclosed in *%{* and
*}*, for example *%{NAME}*, are used as placeholders. A string written in this
syntax is called a _query format_.

Query formats are currently accepted in the following places:

. Query operations with *rpm*(8), as a way to control the output (see
  *--queryformat*)
. *rpm-scriptlets*(7), to access arbitrary header data at runtime (see *-q*)
. Configuration macros in *rpm-config*(5) and *rpmbuild-config*(5), such as
  *%\_query\_all\_fmt* or *%\_rpmfilename*

See the *EXAMPLES* section for more details.

# SYNTAX

A valid query format is composed of optional literal text and zero or more
_placeholders_. A placeholder has the following syntax:

```
%[WIDTH]{TAG[:FORMAT]}
```

When RPM produces output according to a query format, it substitutes each
placeholder with the _TAG_ data from the queried package and formats the data
according to the specified _WIDTH_ and _FORMAT_ (if given). The rest of the
query format is copied to the output unchanged.

Special characters such as *\\n* or *\\t*, with the exception of *\\0*, are also
supported. Curly braces, the percent sign and the backslash may be escaped with
a backslash.

The individual components of a placeholder, as well as some additional language
features, are described in the subsections below.

## Tags

Every RPM package contains a key-value store (the _header_) that represents the
package's properties, such as its name, version or file list. The keys in this
store are called _tags_, and can be used in a query format as placeholders for
the values (_data_) they point to. The data is either a single _scalar_ (string,
integer or binary blob), or an _array_ (argv) of multiple scalars of the same
type.

To obtain a list of all tags supported by the installed version of RPM, run the
following command:

```
rpm -v --querytags
```

This will print a list of available tags, along with their internal numbers and
types, for example:

```
BASENAMES              1117 argv
BUILDHOST              1007 string
BUILDTIME              1006 int32
DESCRIPTION            1005 i18nstring
EPOCH                  1003 int32
INSTALLTIME            1008 int32
NAME                   1000 string
RELEASE                1002 string
SIZE                   1009 int32
SUMMARY                1004 i18nstring
VERSION                1001 string
```

Tags are case-insensitive. Each tag also has an implicit variant with the
*RPMTAG_* prefix, such as *RPMTAG_NAME*. Both variants can be used in a query
format interchangeably.

See the tags documentation in *SEE ALSO* for more details.

## Formats

Each tag is printed in a specific output _FORMAT_. When no format is specified,
the default *:string* is used which displays a bare-bones representation of the
data. You can specify a different format by appending its name after a colon,
for example:

```
%{INSTALLTIME:date}
```

This is useful for tags that are not stored in human-readable form, such as
sizes, dates or signatures. Some formats allow for performing more complex
transformations of the data, such as encoding it in base64 or wrapping it in
JSON syntax.

The following formats are currently available:

|[ *Name*
:< *Description*
|  *:armor*
:  Wrap a public key in ASCII armor
|  *:arraysize*
:  Display number of elements in array tags
|  *:base64*
:  Encode binary data using base64
|  *:date*
:  Use *strftime*(3) "%c" format
|  *:day*
:  Use *strftime*(3) "%a %b %d %Y" format
|  *:depflags*
:  Format dependency comparison operator
|  *:deptype*
:  Format dependency type
|  *:expand*
:  Perform macro expansion
|  *:fflags*
:  Format file flags
|  *:fstate*
:  Format file state
|  *:fstatus*
:  Format file verify status
|  *:hashalgo*
:  Display hash algorithm name
|  *:hex*
:  Format in hexadecimal
|  *:octal*
:  Format in octal
|  *:humaniec*
:  Human readable number (in IEC 80000) where K = 1024, M = 1048576, ...
|  *:humansi*
:  Human readable number (in SI) where K = 1000, M = 1000000, ...
|  *:json*
:  Wrap data in JSON
|  *:perms*
:  Format file permissions
|  *:pgpsig*
:  Display signature fingerprint and time
|  *:shescape*
:  Escape single quotes for use in a script
|  *:string*
:  Display string format (default)
|  *:tagname*
:  Display tag name
|  *:tagnum*
:  Display tag number
|  *:triggertype*
:  Display trigger suffix
|  *:vflags*
:  Format file verification flags
|  *:xml*
:  Wrap data in simple XML markup

Note that some formats are type specific, which means they can only be used with
a specific tag type. If you attempt to use them with an incompatible type, RPM
will replace them with a placeholder text. For example, the following query
format will result in the output "(not a number)":

```
%{NAME:date}
```

Similarly, if a tag is not present in the queried package, it will be replaced
with the text "(none)".

## Width

You can specify the minimum _WIDTH_ of the data when printed, by placing a
number in between the *%* and *{* characters, similarly to field width used in
*printf*(3). Optionally, prefix the number with the minus sign to left-justify
the text.

## Iterators

Tags pointing to arrays may be expanded to the individual elements by using
_iterators_. An iterator is a query (sub-)format enclosed in square brackets
that contains a placeholder for an array, for example:

```
[%{FILENAMES}\\n]
```

RPM will loop through the enclosed array, outputting the (sub-)format each turn
and substituting the placeholder with the current element. This example includes
a newline character, which will result in one filename per line.

Multiple arrays may be placed into a single iterator. RPM will then iterate over
all of them in lockstep, meaning that the first iteration will substitute each
placeholder with its array's first element, the second iteration will use each
array's second element, and so on. For example:

```
[%10{FILESIZES} %{FILENAMES}\\n]
```

This is useful with _parallel_ arrays that RPM keeps internally. These are sets
of arrays where all elements at the same index correspond to each other. One can
think of parallel arrays as table columns that can be arbitrarily rearranged.
The above example will output a table of files and their sizes.

Sometimes, it is useful to combine an array with one or more scalars in the same
iterator. This is possible since RPM internally treats all scalars as arrays of
size one. However, iterators may only contain arrays of the same size, otherwise
RPM will complain. To avoid that, you can "lock" an array so that only its first
element is used in each iteration, by prefixing the tag with the equal sign, for
example:

```
[%{=NAME} %{FILENAMES}\\n]
```

This will produce an "annotated" file list where each filename is preceded by
the name of the package that contains it.

## Expressions

Simple conditionals may be evaluated through query _expressions_ in the form of

```
%|EXPR|
```

where _EXPR_ is an expression.

The only type of expression currently supported is a C-like ternary operator for
simple if/then/else conditions, which has the syntax

```
%|TAG?{PRESENT}:{ABSENT}|
```

where _PRESENT_ and _ABSENT_ are query formats that should be expanded if the
queried package does and doesn't have the _TAG_, respectively. Thus, expressions
can be nested.

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
	Replace _VERSION_ with the version of RPM installed on your system.

## Example 3. Transaction scriptlets

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

See *rpm-scriptlets*(7) for more information on scriptlets in general.

# SEE ALSO

*rpm*(8), *rpm-common*(8), *rpm-config*(5), *rpm-scriptlets*(7), *rpm-spec*(5), *popt*(3)

*https://rpm.org/docs/latest/manual/tags*++
