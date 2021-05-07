---
layout: default
title: rpm.org - Query formats
---
# Query formats

As it is impossible to please everyone with one style of query output, RPM
allows you to specify what information should be printed during a query
operation and how it should be formatted.

## Tags

All of the information a package contains, apart from signatures and the
actual files, is in a part of the package called the header. Each piece
of information in the header has a tag associated with it which allows
RPM to to tell the difference between the name and description of a
package.

To get a list of all of the tags your version of RPM knows about, run the
command 'rpm --querytags'. It will print out a list like (but much longer
then) this:

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

Each of these tags also has a version with a RPMTAG_ prefix, such as
RPMTAG_NAME. You can use this tags with or without the RPMTAG_ prefix.
More detailed documentation about the tags can be found in the
[tag documentation](tags.md).

A tag can consist of one element or an array of elements. Each element can
be a string or number only.

## Query Formats

A query format is passed to RPM after the --queryformat argument, and normally
should be enclosed in quotes. This query format is then used to print
the information section of a query.

The query format is similar to a C style printf string, which the printf(2)
man page provides a good introduction to. However, as RPM already knows the
type of data that is being printed, you must omit the type specifier. In
its place put the tag name you wish to print enclosed in curly braces
({}). For example, the following RPM command prints the names and sizes
of all of the packages installed on a system:

```
    rpm -qa --queryformat "%{NAME} %{SIZE}\n"
```

If you want to use printf formatters, they go between the % and {. To
change the above command to print the NAME in the first 30 bytes and
right align the size to, use:

```
    rpm -qa --queryformat "%-30{NAME} %10{SIZE}\n"
```

## Arrays

RPM uses many parallel arrays internally. For example, file sizes and 
file names are kept as an array of numbers and an array of strings
respectively, with the first element in the size array corresponding
to the first element in the name array. 

To iterate over a set of parallel arrays, enclose the format to be used
to print each item in the array within square brackets ([]). For example,
to print all of the files and their sizes in the slang-devel package
followed by their sizes, with one file per line, use this command:

```
    rpm -q --queryformat "[%-50{FILENAMES} %10{FILESIZES}\n]" slang-devel
```

Note that since the trailing newline is inside of the square brackets, one
newline is printed for each filename.

A popular query format to try to construct is one that prints the
name of a package and the name of a file it contains on one line, 
repeated for every file in the package. This query can be very useful
for passing information to any program that's line oriented (such as
grep or awk). If you try the obvious,

```
    rpm -q --queryformat "[%{NAME} %{FILENAMES}\n]" cdp
```

you'll see RPM complain about an "array iterator used with different
sized arrays". Internally, all items in RPM are actually arrays, so the
NAME is a string array containing one element. When you tell RPM to iterate
over the NAME and FILENAMES elements, RPM notices the two tags have
different numbers of elements and complains.

To make this work properly, you need to tell RPM to always print the first
item in the NAME element. You do this by placing a '=' before the tag
name, like this:

```
    rpm -q --queryformat "[%{=NAME} %{FILENAMES}\n]" cdp
```

which will give you the expected output.

```
    cdp /usr/bin/cdp
    cdp /usr/bin/cdplay
    cdp /usr/man/man1/cdp.1
```

## Formatting Tags

One of the weaknesses with query formats is that it doesn't recognize
that the INSTALLTIME tag (for example) should be printed as a date instead
of as a number. To compensate, you can specify one of a few different
formats to use when printing tags by placing a colon followed the formatting 
name after the tag name. Here are some examples:

```
    rpm -q --queryformat "%{NAME} %{INSTALLTIME:date}\n" fileutils
    rpm -q --queryformat "[%{FILEMODES:perms} %{FILENAMES}\n]" rpm
    rpm -q --queryformat \
	"[%{REQUIRENAME} %{REQUIREFLAGS:depflags} %{REQUIREVERSION}\n]" \
	vlock
```

The :shescape may be used on plain strings to get a string which can pass
through a single level of shell and give the original string.

Formatting names "humansi" and "humaniec" display a number in a human
readable format in SI resp IEC 80000 standard.
humansi uses 1K = 1000, 1M = 1000000, ...
humaniec uses 1K = 1024, 1M = 1048576, ...

The formatting tags are documented in rpm (8) manual in the QUERY OPTIONS
section.

## Query Expressions

Simple conditionals may be evaluated through query expressions. Expressions
are delimited by %|...|. The only type of expression currently supported
is a C-like ternary conditional, which provides simple if/then/else
conditions. For example, the following query format display "present" if
the SOMETAG tag is present, and "missing" otherwise:

```
    %|SOMETAG?{present}:{missing}|
```

Notice that the subformats "present" and "missing" must be inside of curly
braces.

```
    rpm -qa --queryformat "%-30{NAME} %|PREINPROG?{ %{PREINPROG\}}:{no}|\n"
```
