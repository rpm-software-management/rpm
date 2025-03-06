RPMCOMMAND(1)

# NAME
rpmcmd - Example rpm man page

# SYNOPSIS
*rpmcmd* [options] [_ARGUMENT_] ...

# DESCRIPTION
Describe what *rpmcmd* command does in detail.

Put distinct operational modes to their own subsections.

Typographic conventions:
- Use bold for literal values such as command name and options:
*cmd* does X, with option *--do-this* does Y instead
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

# OPTIONS
Describe common options and in this section.

Typographic conventions:
- Put option aliases on separate lines
- Put option arguments with pre-determined values into <> (see below)

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

## MODE OPTIONS
Where applicable:
Put subcommand style modes (common in rpm) into their own subsections,
each describing options specific to that mode.

## CLASS OPTIONS
Put options common to more than one mode into a subsection of its own.

# ENVIRONMENT
Describe environment variables affecting program execution.

# EXIT STATUS
Describe exit codes of the command.

# EXAMPLES
Add at least one example for each major functionality of the command.

```
rpmcmd --do-this package-1.0-1.noarch.rpm
```

# FILES
Describe files specific to this command, or remove if there are none.

# SEE ALSO
*rpm*(8) *popt*(3)

*http://www.rpm.org/*
