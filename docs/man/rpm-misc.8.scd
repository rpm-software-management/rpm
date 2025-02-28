RPMMISC(8)

# NAME

rpm - lesser need options for rpm(8)

# OPTIONS

*--predefine*='_MACRO_ _EXPR_'
	Defines _MACRO_ with value _EXPR_ before loading macro files.


*--color [never|auto|always]*
	Use terminal colors for highlighting error and debug message.
	Default is turning colors on for ttys only (*auto*).

##  Switching off features

*--nocontexts*
	Disable the SELinux plugin if available. This stops
	the plugin from setting SELinux contexts for files and scriptlets.

*--noglob*
	Do not glob arguments when installing package files.

*--nocaps*
	Don't verify capabilities of files.

*--excludeconfigs*, *--noconfigs*
	Do not install configuration files.

*--nohdrchk*
	Don't verify database header(s) when retrieved.

## Debugging

*-d*, *--debug*
	Print debugging information.

*--deploops*
	Print dependency loops as warning.

*--fsmdebug*
	Print debuging information of payload handling code.

*--rpmfcdebug*
	Print debug information about files packaged.

*--rpmiodebug*
	Print debug information about file IO.

*--stats*
	Print runtime statistics of often used functions.

## Obsolete Options

*-K*, *--checksig*
	See and use *rpmkeys*(8).

*--nodocs*
	Do not install documentation. Use *--excludedocs* instead.

