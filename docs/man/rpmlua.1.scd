RPMLUA(1)

# NAME
rpmlua - RPM Lua interpreter

# SYNOPSIS
*rpmlua* [options] [_LUA_SCRIPT_] [-- _ARG_ ...]

# DESCRIPTION
Run RPM internal Lua interpreter.

Note: indexes start at 1 in Lua, so the program name is at arg[1] instead
of the more customary index zero.

# ARGUMENTS
_LUA SCRIPT_
	A Lua script.

_ARG_
	Options and arguments to be passed to _SCRIPT_FILE_.
	*rpmlua* stops processing at *--*.

# OPTIONS
*-i*,
*--interactive*
	Run an interactive session after executing optional script or
	statement.

*--opts*=_OPTSTRING_
	Perform *getopt*(3) option processing on the passed arguments
	according to _OPTSTRING_.

*-e* "_STATEMENT_",
*--execute* "_STATEMENT_"
	Execute a Lua statement before executing optional script.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmlua test.lua*
	Execute test.lua script file.

*rpmlua --opts=ab:c args.lua -- 1 2 3 -c -b5*
	Execute args.lua script file with option processing.

*rpmlua -e "print(rpm.ver('1.0') < rpm.ver('2.0'))"*
	Execute single statement to compare rpm versions.

*rpmlua -i*
	Run an interactive session.

# SEE ALSO
*lua*(1), *popt*(3), *getopt*(3), *rpm*(8) *rpm-lua*(7)

*http://www.rpm.org/*
