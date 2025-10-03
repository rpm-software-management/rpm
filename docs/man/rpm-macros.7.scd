RPM-MACROS(7)

# NAME
rpm-macros - RPM macro processor

# SYNOPSIS
## Defining
*%*_NAME_ _BODY_

*%*_NAME_*(*[_OPTIONS_]*)* _BODY_

## Expanding
*%*_NAME_

*%*_NAME_ [_OPTIONS_] [_ARGUMENTS_]

*%{*_NAME_*}*

*%{*_NAME_ [_OPTIONS_] _ARGUMENTS_*}*

*%{*_NAME_*:*_ARGUMENT_*}*

*%{?*_NAME_*}*

*%{?*_NAME_*:*_VALUE-IF-DEFINED_*}*

*%{!?*_NAME_*:*_VALUE-IF-NOT-DEFINED_*}*

*%(*_SHELL-COMMAND_*)*

*%[*_EXPRESSION_*]*

*%[*_EXPRESSION_ *?* _VALUE-IF-TRUE_ *:* _VALUE-IF-FALSE_*]*

*%{lua:*_LUA-CODE_*}*

# DESCRIPTION

RPM has a powerful built-in macro processor. The primary uses of macros
are configuration and other utility functions for RPM itself,
and as a packaging aid in spec files.

In addition to simple text substitution, the macro processor supports
the following facilities:
- function-like *PARAMETRIC MACROS* with options and arguments processing
  and locally scoped automatic and user-defined macros
- *Shell expansion*
- *Expression expansion*
- *Lua expansion* for embedded Lua processing
- various *BUILT-IN MACROS* for string processing and OS interaction

The syntax for defining simple macros is:

	_NAME_ _BODY_

All whitespace surrounding _BODY_ is removed.  _NAME_ may be composed
of alphanumeric characters and the underscore (*\_*), and must be at least
two characters in length. The body is (re-)expanded on each macro invocation.
Macro names and options are case-sensitive.

See *PARAMETRIC MACROS* for the more advanced macro variant with options
and arguments processing.

Macros can be defined via *rpm-macrofile*(5) files, and fully managed with
macro primitives *%define*, *%global* and *%undefine*, RPM command-line
described in *rpm-common*(8) and the API (C, Python, Lua).

Except for those defined inside parametric macros, macros are always global
in scope.

RPM macros are stacked, ie. when redefining an already existing
macro, it shadows the previous definition instead of replacing it, and
undefining a macro only pops the topmost definition, thus activating
the previous macro definition.

Note that this manual only describes the macro processor engine itself.
On a normal RPM based system, there are a vast number of other macros
defined through *rpm-macrofile*(5) files that will not be covered here.

# EXPANSION
To expand a macro, place *%* in front of it. Several forms are supported:

*%*_NAME_
	Expand the macro _NAME_.

*%{*_NAME_*}*
	Expand the macro _NAME_, allowing placement
	adjacent to other text (similar to *${ENV}* in shell).

*%*_NAME_ [_OPTIONS_] [_ARGUMENTS_]
	Expand the parametric/built-in macro _NAME_, using options
	and arguments parsed up to the string end or next newline.

	*--* can be used to separate options from arguments.

*%{*_NAME_ [_OPTIONS_] [_ARGUMENTS_]*}*
	Expand the parametric/built-in macro _NAME_, using options
	and arguments parsed up to the closing *}*.
	Allows usage adjacent to other text.

*%{*_NAME_*:*_ARGUMENT_*}*
	Expand the parametric/built-in macro _NAME_, using the string after *:*
	as the sole argument. Allows usage adjacent to other text.

Note: The syntaxes for calling parametric and built-in macros are generally
interchangeable now, but prior to 4.18, the *%{*_NAME_*:*_ARGUMENT_*}*
syntax was exclusive to built-in macros.

Macro expansion can be escaped by placing a second *%* in front of the
macro, for example *%%{name}* would be expanded to *%{name}*.

Attempting to expand an undefined macro expands to the
literal invocation, e.g. *%\_undefined* expands to *%\_undefined*.
If this is not desired, use conditionals.

Macro expansions can recurse up to 64 levels.

## Shell expansion
Shell expansion can be performed using *%(*_shell command_*)*.
_shell_command_ is expanded before executing it with _/bin/sh_, whose output
becomes the expansion of the macro. The trailing newline is deleted.

Example:
```
%(echo aa-bb-cc | tr '-' '.')
```

## Conditional expansion
The macro processor supports testing whether a macro is defined or not.

*%{?*_NAME_*:*_VALUE_*}*
	Expands to the expansion of _VALUE_ if _NAME_ is defined, otherwise
	to an empty string.

*%{!?*_NAME_*:*_VALUE_*}*
	Expands to the expansion of _VALUE_ if _NAME_ is _not_ defined,
	otherwise it expands to an empty string.

*%{?*_NAME_*}*
	Shorthand for *%{?*_NAME_*:%{*_NAME_*}}*.

For more complex tests, use *Expression expansion* or *Lua expansion*.
Note that *%if*, *%ifarch* and the like are not macros, they are spec
directives and only usable in that context.

Note that in RPM >= 4.17, conditionals on built-in macros simply test for
existence of that built-in, just like with any other macros.
In older versions, the behavior of conditionals on built-ins is undefined.

## Expression expansion
Expression expansion can be performed using *%[*_EXPRESSION_*]*.  An
expression consists of terms that can be combined using
operators.

RPM supports three kinds of terms:
- numbers made up from digits
- strings enclosed in double quotes (e.g *"somestring"*)
- versions enclosed in double quotes preceded by *v* (e.g *v"3:1.2-1"*)

RPM will expand macros when evaluating terms.

You can use the standard operators to combine terms:
- logical operators *&&*, *||*, *!*
- relational operators *!=*, *==*, *<*, *>*, *<=*, *>=*
- arithmetic operators *+*, *-*, */*, *\**,
- the ternary operator *? :*
- parentheses

For example, *%[ 3 + 4 \* (1 + %two) ]* will expand
to *15* if *%two* expands to *2*. Version terms are compared using
RPM version ([_epoch_:]_version_[-_release_]) comparison algorithm,
rather than regular string comparison.

Note that the *%[*_EXPRESSION_*]* expansion is different to the
*%{expr:*_EXPRESSION_*}* macro.  With the latter, the macros in the
expression are expanded first and then the expression is
evaluated (without re-expanding the terms).  Thus

```
rpm --define 'foo 1 + 2' --eval '%{expr:%foo}'
```

will print *3*.  Using *%[%foo]* instead will result in the
error that "1 + 2" is not a number.

Doing the macro expansion when evaluating the terms has two
advantages.  First, it allows RPM to do correct short-circuit
processing when evaluating logical operators.  Second, the
expansion result does not influence the expression parsing,
e.g. *%["%file"]* will even work if the *%file* macro expands
to a string that contains a double quote.

Added: 4.16.0

## Lua expansion
The most powerful of the macro expansion methods is using RPM's embedded Lua
interpreter:

*%{lua:*_LUA-CODE_*}*
	Execute _LUA-CODE_ using RPM's embedded Lua interpreter, expanding
	to the code's *print()*'ed output.

See *rpm-lua*(7) for the details.

# PARAMETRIC MACROS
Parametric macros are a powerful mechanism that allows building function-like
utility macros with option processing and accepting a variable number of
arguments, much like common shell tools.

The syntax for defining parametric macros is:

	_NAME_([_OPTIONS_]) _BODY_

If present, the _OPTIONS_ (i.e. the string between parentheses) are passed
exactly as is to *getopt*(3) for argc/argv processing at the beginning of
a macro invocation. Only short options are supported.

*-* as the sole _OPTIONS_ field disables RPM's option processing.
This allows macros to fully decide how to handle their input, e.g.
if the arguments of the macro only/mostly consist of items starting
with *-*, the default processing only gets in the way.

## Automatic macros
While a parameterized macro is being expanded, the following shell-like
automatic macros are available:

[[ *Macro*
:< *Description*
|  *%0*
:  the name of the macro being invoked
|  *%\**
:  all arguments (unlike shell, not including any processed flags)
|  *%\*\**
:  all arguments (including any processed flags)
|  *%#*
:  the number of arguments
|  *%{-f}*
:  if present at invocation, the last occurrence of flag *f* (flag and argument)
|  *%{-f\*}*
:  if present at invocation, the argument to the last occurrence of flag *f*
|  *%1*, *%2*, ...
:  the arguments themselves (after *getopt*(3) processing)

If the built-in option processing was disabled with *-* as the _OPTIONS_
field, only the following automatic macros are available:

[[ *Macro*
:< *Description*
|  *%0*
:  the name of the macro being invoked
|  *%\**, *%\*\**
:  all arguments
|  *%#*
:   the number of arguments
|  *%1*, *%2*, ...
:  the arguments themselves

Automatic macros are automatically defined and undefined on parametric
macro entry and exit.

## Accessing options
Within the body of a parametric macro, there are several constructs that
permit testing for the presence of optional parameters. The simplest construct
is *%{-f}* which expands (literally) to *-f* if *-f* was mentioned when the
macro was invoked. There are also provisions for including text if a flag
was present using *%{-f:X}*. This macro expands to (the expansion of) *X*
if the flag was present. The negative form, *%{!-f:Y}*, expanding to (the
expansion of)* Y* if *-f* was *not* present, is also supported.

## Scope and visibility
In general, macros have a global scope, regardless of where and how they were
defined. However, macros defined inside parametric macros have non-global
scope as follows:
- automatic macros have local scope, ie. are only visible on the call-level
  of the macro itself
- user-defined local macros have nested scope, ie. are visible on the
  call-level of the macro itself and deeper

That is, a parametric macro cannot see the options or arguments of another one,
but a user-defined local macro in a calling macro can be accessed in the
callee(s).

To define a global macro inside a parametric macro, you _must_ use *%global*
instead of *%define*. Also note that because such a macro may be referring
to other macros only visible in the current scope, *%global* _expands the
macro body once at the time of definition_.

## Calling convention
When a parametric macro is expanded, the following calling convention is used:
. any arguments to the macro are expanded on the call-level of the callee
. any options to the macro are processed
. automatic macros are set up for the options and the arguments
. the macro body is recursively expanded
. all macros defined on this call-level are discarded

# BUILT-IN MACROS
RPM supports the following built-in macros for various operations.
Built-in macros cannot be undefined or overridden.

Note: The *%{name:arg}* style is used here as it's the most backwards
compatible and does not require quoting for whitespace, but it can generally
be replaced with the other expansion forms too. Built-ins taking multiple
arguments must use other styles, as indicated below.

## Macro manipulation
The macro primitives are used for macro manipulation in spec files
and other macros. Note that all these operate on the macro name _without_
the preceding *%*-character. 

*%define* _NAME_[([_OPTIONS_])] _BODY_
	This is the primary way to define macros.
	A *%define* is always fully declarative: no macro expansion takes place,
	and it has no side-effects. Macros defined with it are in global scope,
	unless the definition occurs inside a parametric macro.

	Example:
	```
	%define mypath /usr/bin/mine
	```

*%global* _NAME_[([_OPTIONS_])] _BODY_
	The *%global* primitive is identical in syntax to *%define*, but has
	two critically important behavioral differences: as it's name suggests,
	a macro defined with *%global* always has a global scope regardless
	of where it's used.

	The second difference is that the _BODY_ is expanded once at the time
	of definition and the expansion becomes the actual macro body.
	Thus, arbitrary code execution and side-effects may
	occur when *%global* is used, depending on the contents and the
	other macros used in _BODY_. The latter can be handy for avoiding
	redundant, possibly expensive macro expansions if the value does not
	change, but be aware of the side-effects.

	Note that while *%global* technically accepts an _OPTIONS_ field,
	it is ill-suited for defining parametric macros because of the
	_BODY_ expansion behavior. 

	Example:
	```
	%global snapver 0-0.48.20240616git
	```

*%undefine* _NAME_
	Note that *%undefine* only pops a macro definition from the stack, so
	using it does _not_ guarantee that _NAME_ is undefined after calling
	*%undefine* on it.
	Automatic macros and built-in macros cannot be undefined.

	Example:
	```
	%undefine mypath
	```

*%{load:*_FILE_*}*
	Load an *rpm-macrofile*(5) file. (Added: 4.12.0)

	Example:
	```
	%{load:/some/dir/macros.foo}
	```

## Macro expansion
*%{expand:*_BODY_*}*
	Expand _BODY_ as if it were a macro body. Useful for increased
	indirection, such as to expand a macro name constructed from
	two or more macros.

	Example:
	```
	%{expand:%{foo_prefix}%{foo_suffix}}
	```

*%{expr:*_EXPRESSION_}
	Expand _EXPRESSION_. See *Expression expansion*. (Added: 4.15.0)

	Example:
	```
	%{expr:5\*1024}
	```

*%{lua:*_LUA-CODE_*}*
	Expand to output of _LUA-CODE_ using the embedded Lua interpreter.
	See *Lua expansion*.

	Example:
	```
	%{lua:for i=65,90 do print(string.char(i)) end}
	```

*%{macrobody:*_NAME_*}*
	Expand to the literal body of the macro _NAME_. (Added: 4.16.0)

	Example:
	```
	%{macrobody:\_libdir}
	```

## String operations
*%dnl*
	Discard to next line (without expanding anything).
	*%dnl* is the recommended way to comment out things in spec files
	because it works everywhere and disables macro processing for
	that line. (Added: 4.15.0)

	Example:
	```
	%dnl This is a comment on %{mymacro} behavior
	```

*%{gsub *_STRING_, _PATTERN_, _REPL_ [,_N_]*}*
	Replace all (or _N_ first if given) occurrences of _PATTERN_ in
	_STRING_ by _REPL_.

	Added: 4.19.0

	Example:
	```
	%{gsub aabbaacc aa dd 1}
	```

*%{len:*_STRING_*}*
	Expand to length of _STRING_. (Added: 4.19.0)

	Example:
	```
	%{len:9bf7da058a7c582878310e75be3d56a5a8b67f95}
	```

*%{lower:*_STRING_*}*
	Expand to lowercase _STRING_. (Added: 4.19.0)

	Example:
	```
	%{lower:CamelCase}'
	```

*%{quote:*_STRING_*}*
	Quote arguments for passing empty strings and strings with
	embedded whitespace as parametric macro arguments. (Added: 4.14.0)

	Example:
	```
	%myzip -x %{quote:empty spaces.zip}
	```

*%{rep *_STRING_, _N_ [,_SEP_]*}*
	Expand to a string that is the concatenation of _N_ copies of
	_STRING_, separated by _SEP_ (if specified).

	Added: 4.19.0

	Example:
	```
	%{rep a 5}
	```


*%{reverse:*_STRING_*}*
	Reverse a string. (Added: 4.19.0)

	Example:
	```
	%{reverse:tac}
	```

*%{shescape:*_STRING_*}*
	Single quote _STRING_ with escapes for use in shell. (Added: 4.18.0)

	Example:
	```
	%{shescape:foo's}
	```

*%{shrink:*_STRING_*}*
	Trim leading and trailing whitespace from _STRING_, reduce
	intermediate whitespace to a single space. (Added: 4.14.0)

	Example:
	```
	%{shrink:aa  bb   ccc   }
	```

*%{span:*_STRING_*}*
	As-is string, handy for wrapping multiline macros. (Added: 6.0.0)

	Example:
	```
	%{span:
		%one thing
		%another thing
	}
	```

*%{sub *_STRING_, _I_, [,_J_]*}*
	Expand to substring of _STRING_ that starts at *I* and continues
	until *J*. *I* and *J* can be negative to index from the string's end.
	If *J* is absent, then it is assumed to be equal to *-1*
	(ie. string end).

	Added: 4.19.0

	Example:
	```
	*%{sub myfile.zip 3 6}*
	```

*%{upper:*_STRING_*}*
	Expand to uppercase _STRING_. (Added: 4.19.0)

	Example:
	```
	%{upper:CamelCase}'
	```

## File and path operations
*%{basename:*_PATH_*}*
	*basename*(1) macro analogue.

	Example:
	```
	%{basename:/some/dir/file.suf}
	```

*%{dirname:*_PATH_*}*
	*dirname*(1) macro analogue.

	Example:
	```
	%{dirname:/some/dir/file.suf}
	```

*%{exists:*_PATH_*}*
	Test file existence, expands to 1/0. (Added: 4.18.0)

	Example:
	```
	%{exists:%{builddir}/myflag.txt}
	```

*%{suffix:*_PATH_*}*
	Expand to suffix part of a filename.

	Example:
	```
	%{suffix:myfile.zip}
	```

*%{url2path:*_URL_*}*
	Convert url to a local path.

	Example:
	```
	%{url2path:http://rpm.org/not/there}
	```

*%{uncompress:*_PATH_*}*
	Expand to a command for outputting argument _PATH_ to stdout,
	uncompressing as needed.

	Example:
	```
	%{uncompress /my/source.tar.gz}
	```

*%{xdg:*_KIND_*}*
	Expand to XDG base directory paths. Supported values for _KIND_ are:
	- *cache*: user-specific non-essential (cached) data
	- *config*: user-specific configuration files
	- *data*: user-specific data files
	- *state*: user-specific state data

	Added: 6.0.0

	Example:
	```
	%{xdg config}
	```

## Environment info
*%getncpus*
	Expand to the number of available CPUs. (Added: 4.15.0)

*%{getncpus:*_KIND_*}*
	Expand to the number of available CPUs, supported valued for _KIND_ are
	- *total*: total number of available CPUs (same as *%getncpus*)
	- *proc*: number of available CPUs for processes
	- *thread*: number of available CPUs for threads

	*proc* and *thread* account for available memory, including
	address space limitations for threads.

	Added: 4.19.0.

	Example:
	```
	%{getncpus proc}
	```

*%getconfdir*
	Expand to RPM "home" directory (typically /usr/lib/rpm).

*%{getenv:*_NAME_*}*
	*getenv*(3) macro analogue.

	Example:
	```
	%{getenv:HOME}
	```

*%rpmversion*
	Expand to running RPM version.

## Output
*%{echo:*_STRING_*}*
	Print _STRING_ to process stdout.

	Example:
	```
	%{echo:Building with foo}
	```

*%{warn:*_STRING_*}*
	Print _STRING_ prefixed with "warning: " to process stderr.

	Example:
	```
	%{warning:Foo is deprecated}
	```

*%{error:*_STRING_*}*
	Print _STRING_ prefixed with "error: " to process stderr and
	flag an error in the macro processor.

	Example:
	```
	%{error:Invalid argument}
	```

*%verbose*
	Expand to 1/0 whether RPM is in verbose mode or not. (Added: 4.17.1)

*%{verbose:*_STRING_*}*
	Expand to _STRING_ if RPM is in verbose mode, the empty string if not.
	(Added: 4.17.1)

	Example:
	```
	%{verbose:-x}
	```

## Spec specific macros
*%{S:*_NUMBER_*}*
	Expand to the filename of the specified Source _NUMBER_.
	*%{S:n}* is equivalent to *%{SOURCEn}*.

*%{P:*_NUMBER_*}*
	Expand to the filename of the specified Patch _NUMBER_.
	*%{P:n}* is equivalent to *%{PATCHn}*.

## Diagnostics
*%trace*
	Toggle print of debugging information before/after expansion.

*%dump*
	Print the active (i.e. non-covered) macro table.

*%\_\_file_name*
	Expand to current filename (if parsing a file). (Added: 4.15)

*%\_\_file_lineno*
	Expand to current line number in current file (if parsing a file).
	(Added: 4.15)

# EXAMPLES
## Example 1. Define a simple macro
Define macro *mylib* to a path relative to *%{\_libdir}* macro in a spec:

```
%define mylib %{_libdir}/mine
```

## Example 2. Define a parametric macro
Define parametric macro *myhelper* which executes the program
specified by *%myprog* with it's first argument and always passing
the option *--some-opt* to it, and additionally the *--xtra* option
if it received the *-x* option itself:

```
%define myhelper(x) %{myprog} --some-opt %{?-x:--xtra} %{1}
```

## Example 3. Define a macro utilizing shell expansion
Define macro *%today* that expands to the current date in _YYMMDD_
format by calling the *date*(1) shell utility.
Note the 2nd *%* needed to escape the arguments to *date*(1):

```
%define today %(date +%%y%%m%%d)
```

## Example 4. Define a macro conditionally
Define macro *mypath* if it wasn't previously defined:

```
%{!?mypath: %define mypath /some/where}
```

## Example 5. Conditional expansion
Expands to *1* if *use_foo* is defined and *0* otherwise:

```
%{?use_foo:1}%{!?use_foo:0}
```

## Example 6. Expressions
Calculate 5 \* 1024:

```
%[5 * 1024]
```

Expand to literal *true* or *false* depending on a condition:

```
%[1 < 2 ? "true" : "false"]
```

Compare versions, expanding to *1* or *0* on true/false:

```
%[ v"3.1.0-1" < v"1.0~alpha-2" ? 1 : 0]
```

Expands to *1* if *%aa* expands to *5*, otherwise expands to *2*:

```
%[ "%{aa}" == "5" ? 1 : 2]
```

# DEBUGGING
Some useful tools for working with and troubleshooting macros:

*rpm --eval "*_VALUE_*"*
	Expand _VALUE_ on the command line.

*rpm --define "aa 11" --eval "%aa"*
	Define and evaluate a macro on the command line.

*rpm --eval "%global unamer %(uname -r)" --eval "%{macrobody:unamer}"*
	Define and examine macro body using *%global* on the command line.

*rpm --eval "%define unamer %(uname -r)" --eval "%{macrobody:unamer}"*
	Define and examine macro body using *%define* on the command line.

*rpmlua*
	Run an interactive shell in the embedded Lua environment.

*rpmlua -e 'print(macros.defined("\_libdir"))'*
	Using the embedded Lua interpreter in standalone form, print
	print 1/0 whether macro *\_libdir* is defined or not.

*rpmspec --shell*
	Run an interactive shell in the macro environment.

*rpmspec --shell telnet.spec*
	Run an interactive shell in the macro environment after parsing
	*telnet.spec*.

# SEE ALSO
*rpm*(8) *rpm-common*(8) *rpm-macrofile*(5) *rpm-config*(5) *rpm-lua*(7)
*rpmspec*(1) *rpmlua*(1)
