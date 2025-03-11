RPMCOMMAND(1)

# NAME
*rpmcmd* - Example rpm man page

# SYNOPSIS
Use the generic form for one-purpose commands:

*rpmcmd* [options] [_ARGUMENT_] ...

Use one entry per operation for multi-operation (aka mode) commands.
Use {} for the operation switches:

*rpmcmd* {*-b*|*--bardize*} [options] [bardize-options] [_ARGUMENT_] ...

*rpmcmd* {*-f*|*--frobnize*} [options] [_ARGUMENT_] ...

# DESCRIPTION
Describe what *rpmcmd* command does in plain, understandable
English. Command-line switches belong to *OPTIONS* and *OPERATIONS*
as appropriate.

Typographic conventions:
- Use hard wrapping at 80 columns for all text
- Use bold for literal values such as command name and options:
*cmd* does X, with option *--do-this* does Y instead
- Use bold when referring to section names
- Use bold when referring to other man page names, eg *rpm*(8)
- Use underline uppercase to denote user arguments: _ARGUMENT_
- Use underline for literal file names: _hello-1.0-1.noarch.rpm_
- Use alphabetical order whenever multiple things are listed and
  there's no contextual reason to do otherwise
- Put the short variant first when listing option aliases
- Put an empty line before a new section/option/etc
- Put text immediately below a section/option/etc (ie without an empty line)
- Put optional elements into [] brackets
- Do not emphasize separators like ",", "=", "[]", "<>", "|" etc

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

_ARGUMENT_
	File of type X.

# OPTIONS
Describe common options and in this section.

Typographic conventions:
- Put option aliases on separate lines
- Put option arguments with pre-determined values into <> (see below)

See *rpm.common*(8) for the options common to all *rpm* executables.

*-d*,
*--do-this*
	Do Y instead of default action.

*-o* _ARG_,
*--output*=_ARG_
	Use file _ARG_ for output.

*--format*=<*aa*|*bb*>
	Use format *aa* or *bb* for output.

*--compress*=<_method_>
	Use specified compression, where <_method_> is one of the following
	(when there are too many to list directly in the option)
	- *bzip2*: bzip2 compression
	- *gzip*: zlib/gzip compression
	- *xz*: xz compression is a more complicated topic, requiring
	  multiline explanation
	- *zstd*: zstd compression

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
See *rpm.config*(5) for details:
- *%\_bar_mode*
- *%\_foo_mode*

# ENVIRONMENT
Describe environment variables affecting program execution.

# EXIT STATUS
On success, 0 is returned, a non-zero failure code otherwise.

# EXAMPLES
Add at least one example for each major functionality of the command.

```
rpmcmd --do-this package-1.0-1.noarch.rpm
```

# FILES
Describe files specific to this command, or remove if there are none.

# SEE ALSO
*rpm*(8) *rpm.common*(8) *popt*(3)

*http://www.rpm.org/*
