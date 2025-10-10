RPMCOMMAND(1)

# NAME
*rpmcmd* - Example rpm man page

# SYNOPSIS
Use the generic form for one-purpose commands:

*rpmcmd* [options] [_ARGUMENT_] ...

Use one entry per operation for multi-operation (aka mode) commands.
Use {} for the operation switches, or <operation> if the operations
all share similar options and arguments.

*rpmcmd* {*-b*|*--bardize*} [options] [bardize-options] [_ARGUMENT_] ...

*rpmcmd* {*-f*|*--frobnize*} [options] [_ARGUMENT_] ...

*rpmcmd* <operation> [options] [_ARGUMENT_] ...

# DESCRIPTION
Describe what *rpmcmd* command does in plain, understandable
English. Command-line switches belong to *OPTIONS* and *OPERATIONS*
as appropriate.

Typographic conventions:
- Use imperative style when describing functionality
- Use hard wrapping at 80 columns for all text
- Use bold for literal values such as command name and options:
*cmd* does X, with option *--do-this* does Y instead
- Use UPPERCASE for first level sections, and Mixed case
  for second level sections (if present)
- Use matching case and bold when referring to section names
- Use uppercase RPM when referring to the RPM project
- Use bold when referring to other man page names, eg *rpm*(8)
- Use underline uppercase for user arguments placeholders: _ARGUMENT_
- Use surrounding `<>` for multi-value placeholders to differentiate
  from free-form placeholders: <_METHOD_> (eg one of http/ftp)
- Use underline for literal file names: _hello-1.0-1.noarch.rpm_
- Use alphabetical order whenever multiple things are listed and
  there's no contextual reason to do otherwise
- Put the short variant first when listing option aliases
- Put an empty line before a new section/option/etc
- Put text immediately below a section/option/etc (ie without an empty line)
- Put optional elements into [] brackets
- Do not emphasize separators like ",", "=", "[]", "<>", "|" etc
- Use "Added: X.Y.Z." to indicate the version where a feature was added.
  Either in parentheses at the end of a line (inline variant) or on a
  line of it's own, surrounded by empty lines (standalone variant).
  Only use this for versions 4.11.0 and newer.

When in doubt, consult *man-pages*(7).

# OPERATIONS
For multi-operation commands, describe each operation invocation
using same notation as for *OPTIONS*. Delete this section if the
command only has a single main operation.

*-f*,
*--frobnize*,
	Describe frobnization in detail.
	Multiple lines are the expectation.

*-b*,
*--bardize*
	Describe bardization in detail.
	Multiple lines are the expectation.

# ARGUMENTS
Describe positional _ARGUMENT_ placeholders from *SYNOPSIS* here.
If the command takes no arguments, delete this section.

_ARGUMENT_
	File of type X.

Use descriptive and obvious placeholder names: if it's a file,
call the placeholder _FILE_.

# OPTIONS
Describe common options in this section.
If the command takes no options, delete this section.

Typographic conventions:
- Put option aliases on separate lines
- Put option arguments with pre-determined values into <> (see below)

See *rpm-common*(8) for the options common to all *rpm* executables.

*-d*,
*--do-this*
	Do Y instead of default action.

*-o* _FILE_,
*--output*=_FILE_
	Use _FILE_ for output.

*--format*=<_FORMAT_>
	Use _FORMAT_ *aa* or *bb* for output.

*--compress*=<_METHOD_>
	Use specified compression, where <_METHOD_> is one of the following
	(when there are too many to list directly in the option)
	- *bzip2*: bzip2 compression
	- *gzip*: zlib/gzip compression
	- *xz*: xz compression is a more complicated topic, requiring
	  multiline explanation
	- *zstd*: zstd compression (Added: 3.1.2)

	Added: 2.1.3

# BARDIZE OPTIONS
Put operation specific options into a top-level section of their own,
matching [operation-options] placeholder in SYNOPSIS.

# OUTPUT
Describe program output to the degree needed to understand program
operation. Do not make unnecessary promises about the exact
format if the output is not intended to be machine parseable.

Use formal notation in an non-indented literal text block for machine
parseable output:
```
<lineno> "<dependency>"
```

Describe operation-specific output by their main invocation switch,
if necessary.
*--frobnize*
```
<lineno> <deptype> "<dependency>"
```

# CONFIGURATION
List main configurables affecting this program here, but don't describe.
If the command has no relevant configuration, delete this section.

See *rpm-config*(5) for details:
- *%\_bar_mode*
- *%\_foo_mode*

# ENVIRONMENT
Describe environment variables affecting program execution.
If the command has no relevant environment variables, delete this section.

# EXIT STATUS
On success, 0 is returned, a non-zero failure code otherwise.

# EXAMPLES
Add at least one example for each major functionality of the command.
Command first in bold, then description, similar to option descriptions.
If command output is included in the description, use indented literal block.
Use second level, numbered sections to separate longer examples from each other,
or to group related ones together.

## Example 1. Basic commands

*rpmcmd --do-this package-1.0-1.noarch.rpm*
	Do this thing to _package-1.0-1.noarch.rpm_.

*rpmcmd --ls package-1.0-1.noarch.rpm*
	List contents of _package-1.0-1.noarch.rpm_.
	```
	/bin/one
	/lib/two
	```

# FILES
Describe files specific to this command.
Delete this section if there are none.

# SEE ALSO
*rpm*(8) *rpm-common*(8) *rpm-config*(5) *popt*(3)

*http://www.rpm.org/*
