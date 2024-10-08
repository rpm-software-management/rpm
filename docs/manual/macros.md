---
layout: default
title: rpm.org - Macro syntax
---
# Macro syntax

RPM has fully recursive spec file macros.  Simple macros do straight text
substitution. Parameterized macros include an options field, and perform
argc/argv processing on white space separated tokens to the next newline.
During macro expansion, both flags and arguments are available as macros
which are deleted at the end of macro expansion.  Macros can be used
(almost) anywhere in a spec file, and, in particular, in "included file
lists" (i.e. those read in using `%files -f <file>`).  In addition, macros
can be nested, hiding the previous definition for the duration of the
expansion of the macro which contains nested macros.

## Defining a Macro

The macro definition syntax is:

```
%define <name>[(opts)] <body>
```

All whitespace surrounding `<body>` is removed.  Name may be composed
of alphanumeric characters and the character `_`, and must be at least
3 characters in length. The body is (re-)expanded on each macro invocation.

### Parametric macros
A macro which contains an `(opts)` field is called parametric.
The opts (i.e. string between parentheses) are passed exactly as is
to to getopt(3) for argc/argv processing at the beginning of
a macro invocation. Only short options are supported.
`--` can be used to separate options from arguments.
Since rpm >= 4.17 `-` as opts disables all option processing.

Macros `%define`d inside a parametric macro have a scope local to the
macro, otherwise all macros are global.

### Automatic macros

While a parameterized macro is being expanded, the following shell-like
automatic macros are available:

| Macro      | Description
| ---------- | -----------
| `%0`       | the name of the macro being invoked
| `%*`       | all arguments (unlike shell, not including any processed flags)
| `%**`      | all arguments (including any processed flags)
| `%#`       | the number of arguments
| `%{-f}`    | if present at invocation, the last occurence of flag f (flag and argument)
| `%{-f*}`   | if present at invocation, the argument to the last occurence of flag f
| `%1`, `%2`, ...| the arguments themselves (after getopt(3) processing)

With rpm >= 4.17 and disabled option processing the mentioned macros are defined as:

| Macro       | Description 
| ----------- | ----------- 
| `%0`        | the name of the macro being invoked
| `%*`, `%**` | all arguments
| `%#`        | the number of arguments
| `%1`, `%2`, ...| the arguments themselves

At the end of invocation of a parameterized macro, the above macros are
automatically discarded.

### Global macros

A macro can be declared into the global scope as follows:

```
%global <name>[(opts)] <body>
```

An important and useful feature of `%global` is that `<body>` is expanded
at the time of definition, as opposed to time of use with regular macros.
This is important inside parametric macros because otherwise the body could
be referring to macros that are out of scope at the time of use, but also
useful to avoid re-expansion of expensive macros.

## Writing a Macro

Within the body of a macro, there are several constructs that permit
testing for the presence of optional parameters. The simplest construct
is `%{-f}` which expands (literally) to "-f" if `-f` was mentioned when the
macro was invoked. There are also provisions for including text if a flag
was present using `%{-f:X}`. This macro expands to (the expansion of) X
if the flag was present. The negative form, `%{!-f:Y}`, expanding to (the
expansion of) Y if -f was *not* present, is also supported.

In addition to the `%{...}` form, shell expansion can be performed
using `%(shell command)`.

## Builtin Macros

There are several builtin macros (with reserved names) for performing
various common operations.


### Macro manipulation

| Macro               | Description | Introduced
| ------------------- | ----------- | ----------
| `%define ...`       | define a macro |
| `%undefine ...`     | undefine a macro |
| `%global ...`       | define a macro whose body is available in global context |
| `%dnl`              | discard to next line (without expanding) | 4.15.0
| `%{load:...}`       | load a macro file | 4.12.0
| `%{expand:...}`     | like eval, expand ... to \<body> and (re-)expand \<body> |
| `%{expr:...}`       | evaluate an [expression](#expression-expansion) | 4.15.0
| `%{lua:...}`        | expand using the [embedded Lua interpreter](lua.md) |
| `%{macrobody:...}`  | literal body of a macro | 4.16.0
| `%{quote:...}`      | quote a parametric macro argument, needed to pass empty strings or strings with whitespace | 4.14.0

### String operations

| Macro            | Description | Introduced
| ---------------- | ----------- | ----------
| `%{gsub:...}`    | replace occurences of pattern in a string (see Lua `string.gsub()`) | 4.19.0
| `%{len:...}`     | string length | 4.19.0
| `%{lower:...}`   | lowercase a string | 4.19.0
| `%{rep:...}`     | repeat a string (see Lua `string.rep()`) | 4.19.0
| `%{reverse:...}` | reverse a string | 4.19.0
| `%{sub:...}`     | expand to substring (see Lua `string.sub()`) | 4.19.0
| `%{upper:...}`   | uppercase a string | 4.19.0
| `%{shescape:...}`| single quote with escapes for use in shell | 4.18.0
| `%{shrink:...}`  | trim leading and trailing whitespace, reduce intermediate whitespace to a single space | 4.14.0

### File and path operations

| Macro               | Description | Introduced
| ------------------- | ----------- | ----------
| `%{basename:...}`   | basename(1) macro analogue |
| `%{dirname:...}`    | dirname(1) macro analogue | 4.11.0
| `%{exists:...}`     | test file existence, expands to 1/0 | 4.18.0
| `%{suffix:...}`     | expand to suffix part of a file name |
| `%{url2path:...}`   | convert url to a local path |
| `%{uncompress:...}` | expand to a command for outputting argument file to stdout, uncompressing as needed |

### Environment info

| Macro              | Description | Introduced
| ------------------ | ----------- | ----------
| `%getncpus`        | expand to the number of available CPUs | 4.15.0
| `%{getncpus:...}`  | accepts arguments `total`, `proc` and `thread`, additionally accounting for available memory (e.g. address space limitations for threads) | 4.19.0
| `%getconfdir`      | expand to rpm "home" directory (typically /usr/lib/rpm) | 4.7.0
| `%{getenv:...}`    | getenv(3) macro analogue | 4.7.0
| `%rpmversion`      | expand to running rpm version |

### Output

| Macro            | Description | Introduced
| ---------------- | ----------- | ----------
| `%{echo:...}`    | print ... to stdout |
| `%{warn:...}`    | print warning: ... to stderr |
| `%{error:...}`   | print error: ... to stderr and return an error |
| `%verbose`       | expand to 1 if rpm is in verbose mode, 0 if not |
| `%{verbose:...}` | expand to ... if rpm is in verbose mode, the empty string if not | 4.17.1

### Spec specific macros

| Macro            | Description
| ---------------- | -----------
| `%{S:...}`       | expand ... to \<source> file name
| `%{P:...}`       | expand ... to \<patch> file name

### Diagnostics

| Macro            | Description | Introduced
| ---------------- | ----------- | ----------
| `%trace`         | toggle print of debugging information before/after expansion
| `%dump`          | print the active (i.e. non-covered) macro table
| `%__file_name`   | current file (if parsing a file) | 4.15
| `%__file_lineno` | line number in current file (if parsing a file) | 4.15

Macros may also be automatically included from /usr/lib/rpm/macros.
In addition, rpm itself defines numerous macros. To display the current
set, add `%dump` to the beginning of any spec file, process with rpm, and
examine the output from stderr.

## Conditionally Expanded Macros

Sometimes it is useful to test whether a macro is defined or not. Syntax

```
%{?macro_name:value}
%{!?macro_name:value}
```

can be used for this purpose. `%{?macro_name:value}` is expanded to "value"
if "macro_name" is defined, otherwise it is expanded to the empty string.
`%{!?macro_name:value}` negates the test. It is expanded to "value"
if `macro_name` is not defined. Otherwise it is expanded to the empty string.

Frequently used conditionally expanded macros are e.g.
Define a macro if it is not defined:

```
%{!?with_python3: %global with_python3 1}
```

A macro that is expanded to 1 if "with_python3" is defined and 0 otherwise:

```
%{?with_python3:1}%{!?with_python3:0}
```

or shortly

```
0%{?with_python3:1}
```

`%{?macro_name}` is a shortcut for `%{?macro_name:%macro_name}`.

For more complex tests, use [expressions](#expression-expansion) or [Lua](lua).
Note that `%if`, `%ifarch` and the like are not macros, they are spec
directives and only usable in that context.

Note that in rpm >= 4.17, conditionals on built-in macros simply test for
existence of that built-in, just like with any other macros.
In older versions, the behavior of conditionals on built-ins is undefined.

## Example of a Macro

Here is an example `%patch` definition from /usr/lib/rpm/macros:

```
%patch(b:p:P:REz:) \
%define patch_file	%{P:%{-P:%{-P*}}%{!-P:%%PATCH0}} \
%define patch_suffix	%{!-z:%{-b:--suffix %{-b*}}}%{!-b:%{-z:--suffix %{-z*}}}%{!-z:%{!-b: }}%{-z:%{-b:%{error:Can't specify both -z(%{-z*}) and -b(%{-b*})}}} \
	%{uncompress:%patch_file} | patch %{-p:-p%{-p*}} %patch_suffix %{-R} %{-E} \
...
```


The first line defines %patch with its options. The body of %patch is

```
%{uncompress:%patch_file} | patch %{-p:-p%{-p*}} %patch_suffix %{-R} %{-E}
```

The body contains 7 macros, which expand as follows

| Macro              | Meaning
| -----------------  | -------------------
|`%{uncompress:...}` | copy uncompressed patch to stdout
| → `%patch_file`    | ... the name of the patch file
|`%{-p:...}`         | if "-p N" was present, (re-)generate "-pN" flag
| → `-p%{-p*}`       | ... note patch-2.1 insists on contiguous "-pN"
|`%patch_suffix`     | override (default) ".orig" suffix if desired
|`%{-R}`             | supply "-R" (reversed) flag if desired
|`%{-E}`             | supply "-E" (delete empty?) flag if desired


There are two "private" helper macros, created with `%define`:

| Macro           | Meaning
| --------------- | ----------------
| `%patch_file`   |the gory details of generating the patch file name
| `%patch_suffix` |the gory details of overriding the (default) ".orig"


## Using a Macro

To use a macro, write:

```
%<name> ...
```

or

```
%{<name>}
```

The `%{...}` form allows you to place the expansion adjacent to other text.
The `%<name>` form, if a parameterized macro, will do argc/argv processing
of the rest of the line as described above.  Normally you will likely want
to invoke a parameterized macro by using the `%<name>` form so that
parameters are expanded properly.

Example:
```
%define mymacro() (echo -n "My arg is %1" ; sleep %1 ; echo done.)
```

Usage:

```
%mymacro 5
```

This expands to:

```
(echo -n "My arg is 5" ; sleep 5 ; echo done.)
```

This will cause all occurrences of `%1` in the macro definition to be
replaced by the first argument to the macro, but only if the macro
is invoked as `%mymacro 5`.  Invoking as `%{mymacro} 5` will not work
as desired in this case.

## Shell Expansion

Shell expansion can be performed using `%(shell command)`. The expansion
of `%(...)` is the output of (the expansion of) ... fed to /bin/sh.
For example, `%(date +%%y%%m%%d)` expands to the string "YYMMDD" (final
newline is deleted). Note the 2nd `%` needed to escape the arguments to
/bin/date.

## Expression Expansion

Expression expansion can be performed using `%[expression]`.  An
expression consists of terms that can be combined using
operators.  Rpm supports three kinds of terms, numbers made up
from digits, strings enclosed in double quotes (eg `"somestring"`) and
versions enclosed in double quotes preceded by v (eg `v"3:1.2-1"`).
Rpm will expand macros when evaluating terms.

You can use the standard operators to combine terms: logical
operators `&&`, `||`, `!`, relational operators `!=`, `==`, `<`,
`>`, `<=`, `>=`, arithmetic operators `+`, `-`, `/`, `*`,
the ternary operator `? :`, and parentheses.
For example, `%[ 3 + 4 * (1 + %two) ]` will expand
to "15" if `%two` expands to "2". Version terms are compared using
rpm version ([epoch:]version[-release]) comparison algorithm,
rather than regular string comparison.

Note that the `%[expression]` expansion is different to the
`%{expr:expression}` macro.  With the latter, the macros in the
expression are expanded first and then the expression is
evaluated (without re-expanding the terms).  Thus

```
rpm --define 'foo 1 + 2' --eval '%{expr:%foo}'
```

will print "3".  Using `%[%foo]` instead will result in the
error that "1 + 2" is not a number.

Doing the macro expansion when evaluating the terms has two
advantages.  First, it allows rpm to do correct short-circuit
processing when evaluation logical operators.  Second, the
expansion result does not influence the expression parsing,
e.g. `%["%file"]` will even work if the `%file` macro expands
to a string that contains a double quote.

## Command Line Options

When the command line option `--define 'macroname value'` allows the
user to specify the value that a macro should have during the build.
Note lack of leading `%` for the macro name.  We will try to support
users who accidentally type the leading `%` but this should not be
relied upon.

Evaluating a macro can be difficult outside of an rpm execution context. If
you wish to see the expanded value of a macro, you may use the option

```
--eval '<macro expression>'
```

that will read rpm config files and print the macro expansion on stdout.

Note: This works only macros defined in rpm configuration files, not for
macros defined in specfiles. You can use `%{echo: %{your_macro_here}}` if
you wish to see the expansion of a macro defined in a spec file.
 
## Configuration using Macros

Most rpm configuration is done via macros. There are numerous places from
which macros are read, in recent rpm versions the macro path can be seen
with `rpm --showrc|grep "^Macro path"`. If there are multiple definitions
of the same macro, the last one wins.

Per-user macros are always the last in the path. As of rpm >= 4.20, rpm
primarily looks for per-user configuration inside `~/.config/rpm` directory
(as per `XDG_CONFIG_HOME` specification), but falls back to the traditional
`~/.rpmmacros` location if necessary.

The macro file syntax is simply:

```
%<name>		 <body>
```

...where `<name>` is a legal macro name and `<body>` is the body of the macro.
Multiline macros can be defined by shell-like line continuation, ie ``\``
at end of line.

Note that the macro file syntax is strictly declarative, no conditionals
are supported (except of course in the macro body) and no macros are
expanded during macro file read.

## Macro Analogues of Autoconf Variables

Several macro definitions provided by the default rpm macro set have uses in
packaging similar to the autoconf variables that are used in building packages:

| Macro              | Default value
| ------------------ | -------------
| `%_prefix`         | /usr
| `%_exec_prefix`    | %{_prefix}
| `%_bindir`         | %{_exec_prefix}/bin
| `%_sbindir`        | %{_exec_prefix}/sbin
| `%_libexecdir`     | %{_exec_prefix}/libexec
| `%_datadir`        | %{_prefix}/share
| `%_sysconfdir`     | /etc
| `%_sharedstatedir` | %{_prefix}/com
| `%_localstatedir`  | %{_prefix}/var
| `%_libdir`         | %{_exec_prefix}/lib
| `%_includedir`     | %{_prefix}/include
| `%_oldincludedir`  | /usr/include
| `%_infodir`        | %{_datadir}/info
| `%_mandir`         | %{_datadir}/man
