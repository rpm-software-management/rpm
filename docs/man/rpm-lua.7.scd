RPM-LUA(7)

# NAME

rpm-lua - RPM embedded Lua interpreter

# SYNOPSIS

%scriptlet -p <lua>

%{lua:...}

# DESCRIPTION
Lua is a general purpose programming language specifically designed for
embedding in other programs, and RPM includes an embedded Lua interpreter
for use in advanced *rpm-macros*(7) and transaction scriptlets.

The embedded Lua interpreter makes possible various things that are
hard or impossible with plain macros or external shell scripts, such as
help eliminate dependency loops from package scriptlets.

# MACROS
## Accessing macros
The *rpm* extension has various functions for dealing with macros, but
the most convenient way to access *rpm-macroproc*(7) from the RPM Lua
environment is via the global *macros* table.

Lua makes no difference between table index and field name syntaxes so
*macros.foo* and *macros['foo']* are equivalent, use what better suits the
purpose.

Like any real Lua table, non-existent items are returned as *nil*,
and assignment can be used to define or undefine macros.

Example:
```
if not macros.yours then
    macros.my = 'my macro'
end

local v = { '_libdir', '_bindir', '_xbindir' }
for _, v in ipairs(v) do
    if not macros[v] then
        macros[v] = 'default'
    end
end
```

All macros share the same global Lua execution environment.

## Calling parametric macros
Parametric macros (including all built-in macros) can be called in a Lua
native manner via the *macros* table, with either *macros.*_NAME_*()* or
*macros[*_NAME_*]()* syntax.

Arguments are passed through a single argument, which is either
- a single string, in which case it's expanded and split with the
  macro-native rules
- a table, in which case the table contents are used as literal arguments
  that are not expanded in any way

Example 1:
```
macros.with('foo')
```

Example 2:
```
macros.dostuff({'one', 'two', 'three'})
```

## Returning data
By definition, anything *print()*'ed in Lua will end up in the macro
expansion. Lua macros can also *return* their output, which makes
programming helper macros look more natural.

Example:
```
%sum() %{lua:
   local v = 0
   for _, a in ipairs(arg) do
       v = v + tonumber(a)
   end
   return v
}
```

## Options and arguments
Parametric Lua macros receive their options and arguments as two local
tables *opt* and *arg*, where *opt* holds processed option values keyed by
the option character, and *arg* contains arguments numerically indexed.

These tables are always present regardless of whether options or arguments
were actually passed to simplify use.

Example:
```
%foo(a:b) %{lua:
if opt.b then
   print('do b')
else
   print('or not')
end
if opt.a == 's' then
   print('do s')
end
if #arg == 0 then
   print('no arguments :(')
else
   for i = 1, #arg do
      print(arg[i])
   end
end
}
```

# SCRIPTLETS
The internal Lua can be used as the interpreter of RPM any transaction
scriptlets, including triggers and file triggers:

Example:
```
%pre -p <lua>
print('Hello from Lua')
```

While the venerable _/bin/sh_ is usually more convenient for packaging related
scripting activities, the embedded Lua interpreter has some unique advantages
for transaction scriptlets: they add no extra dependencies to the packages
and so can help eliminate dependency loops. This can be a crucial
difference in the early "bootstrap" package set in an initial install.

In particular, an embedded Lua script is the only generally usable option
in *%pretrans* scriplets during the initial installation of a system.

Embedded Lua is also much faster than executing a potentially heavyweight
interpreter just to run a couple of lines of shell script.

Note: scriptlets using the internal Lua should not make assumptions about
sharing the execution environment with other scriptlets.

## Arguments
Scriptlet arguments are accessible from a global *arg* table.

Note: in Lua, indexes customarily start at 1 (one) instead of 0 (zero),
and for the better or worse, the RPM implementation follows this practise.
Thus the scriptlet arg indexes are off by one from general expectation based
on traditional scriptlet arguments. The argument containing number of
installed package instances is *arg[2]* and the similar argument for trigger
targets is *arg[3]*, vs the traditional *$1* and *$2* in shell scripts.

Example:
```
%postun -p <lua>
if arg[2] == 0  then
    print("erasing")
end
```

## Relocatable packages
Scriptlets of relocatable packages additionally carry a global
*RPM_INSTALL_PREFIX* table containing all the possible prefixes of the
package.

Added: 4.18.0

## Exit status
While scriptlets shouldn't be allowed to fail normally, you can signal
scriptlet failure status by using Lua's *error(*_MSG_, [_LEVEL_]*)*
function if you need to.

## File triggers
Other file triggers receive the triggering paths via standard input, but
Lua scriplets need to call the *rpm.next_line()* function in a loop. This
function only exists inside the file trigger running environment.

# SPEC FILES
In the context of a *rpm-spec*(5) file parse with *rpmbuild*(1) or
*rpmspec*(1), the RPM Lua environment contains the following spec specific
global tables:

*patches*
	The filenames of the patches in the spec, in the
	order they appeared in the spec.

*patch_nums*
	The numbers of the patches in the spec, in the
	order they appeared in the spec.

*sources*
	The filenames of the sources in the spec, in the
	order they appeared in the spec.

*source_nums*
	The numbers of the sources in the spec, in the
	order they appeared in the spec.

Example:
```
for i, p in ipairs(patches) do
    print(string.format("echo %d: %s\n", patch_nums[i], patches[i]))
end
```

# EXTENSIONS
In addition to Lua standard libraries (subject to the Lua version RPM
is linked to), the following extensions are available in the RPM internal
Lua interpreter. These can be used in all contexts where the internal
Lua can be used.

## rpm extension

The following RPM specific functions are available:

*b64decode(*_ARG_*)*
	Perform base64 decoding on argument.
	See also b64encode().

	Example:
	```
	blob = 'binary data'
	print(blob)
	e = rpm.b64encode(blob)
	print(e)
	d = rpm.b64decode(e)
	print(d)
	```

*b64encode(*_ARG_ [, _LINELEN_]*)*
	Perform base64 encoding on argument.
	Line length may be optionally specified via second argument.
	See also b64decode().

*define("*_NAME_ _BODY_*")*
	Define a global macro _NAME_ with _BODY_.  See also *MACROS*.

	Example:
	```
	rpm.define('foo 1')
	```

*execute(*_PATH_ [, _ARG1_ [,...]*)*
	Execute an external command.
	This is handy for executing external helper commands without depending
	on the shell. _PATH_ is the command to execute, followed
	by optional number of arguments to pass to the command.

	For a better control over the process execution and output,
	see rpm.spawn().

	Added: 4.15.0

	Example:
	```
	rpm.execute('ls', '-l', '/')
	```
*expand(*_ARG_*)*
	Perform RPM macro expansion on _ARG_ string. See also *MACROS*.

	Example:
	```
	rpm.expand('%{_libdir}/mydir')
	```

*glob(*_PATTERN_, [_FLAGS_]*)*
	Return a table of pathnames matching _PATTERN_.
	If _FLAGS_ contains *c*, return _PATTERN_ in case of no matches.

	Example:
	```
	for i, p in ipairs(rpm.glob('*')) do
	    print(p)
	end
	```

*interactive()*
	Launch interactive session for testing and debugging.
	Use *rpmlua*(1) instead.

	Example:
	```
	rpm --eval "%{lua: rpm.interactive()}"
	```

*isdefined(*_NAME_*)*
	Test whether a macro _NAME_ is defined and whether it's parametric,
	returned in two booleans. See also *MACROS*. (Added: 4.17.0)

	Example:
	```
	if rpm.isdefined('_libdir') then
	    ...
	end
	```

*load(*_PATH_*)*
	Load a macro file from given path.
	Same as the built-in *%{load:...}* macro.

	Example:
	```
	rpm.load('my.macros')
	```

*open(*_PATH_, [_MODE_[._FLAGS_]]*)*
	Open a file stream using RPM IO facilities, with support for
	transparent compression and decompression.

	_PATH_ is filename string, optionally followed with _MODE_ string to
	specify open behavior:
	- *a*: open for append
	- *w*: open for writing, truncate
	- *r*: open for reading (default)
	- *+*: open for reading and writing
	- *x*: fail if file exists

	and optionally followed by *rpm-payloadflags*(7) for compression and
	decompression.

	Added: 4.17.0

	Example:
	```
	f = rpm.open('some.txt.gz', 'r.gzdio')
	print(f:read())
	```

	The returned rpm.fd object has the following methods:

	*fd:close()*

	Close the file stream.

	Example:
	```
	f = rpm.open('file')
	f:close()
	```

	*fd:flush()*

	Flush the file stream.

	Example:
	```
	f = rpm.open('file', 'w')
	f:write('foo')
	f:flush()
	f:close()
	```

	*fd:read(*[_LEN_]*)*

	Read data from the file stream up to _LEN_ bytes or if not specified,
	the entire file.

	Example:
	```
	f = rpm.open('/some/file')
	print(f:read())
	```

	*fd:seek(*_MODE_, _OFFSET_*)*

	Reposition the file offset of the stream. _MODE_ is one of *set*,
	*cur* and *end*, and offset is relative to the mode: absolute,
	relative to current or relative to end. Not all streams support seeking.

	Returns file offset after the operation.

	See also *lseek*(3).

	Example:
	```
	f = rpm.open('newfile', 'w')
	f:seek('set', 555)
	f:close()
	```

	*fd:write(*_BUF_ [, _LEN_]*)*

	Write data in _BUF_ to the file stream, either in its entirety or up
	to _LEN_ bytes if specified.

	Example:
	```
	f = rpm.open('newfile', 'w')
	f:write('data data')
	f:close()
	```

	*fd:reopen(*_MODE_*)*

	Reopen a stream with a new mode (see *rpm.open()*).

	Example:
	```
	rpm.open('some.txt.gz')
	f = f:reopen('r.gzdio')
	print(f:read())}
	```

*redirect2null(*_FDNO_*)* (OBSOLETE)
	Redirect file descriptor fdno to /dev/null
	(prior to 4.16 this was known as posix.redirect2null())

	This function is obsolete and only available for RPM v4 packages
	for backwards compatibility.
	Use `rpm.spawn()` or `rpm.execute()` instead.

	```
	pid = posix.fork()
	if pid == 0 then
	    posix.redirect2null(2)
	    assert(posix.exec('/bin/awk'))
	elseif pid > 0 then
	    posix.wait(pid)
	end
	```

*spawn(*{_COMMAND_} [, {_ACTIONS_}]*)*
	Spawn, aka execute, an external program.

	{_COMMAND_} is a table consisting of the command and its arguments.
	An optional second table can be used to pass various actions related
	to the command execution, currently supported are:

	```
	| Action  | Argument(s) | Description
	|---------|----------------------
	| *stdin* | path        | Redirect standard input to path
	| *stdout*| path        | Redirect standard output to path
	| *stderr*| path        | Redirect standard error to path
	```

	Returns the command exit status: zero on success, or a tuplet
	of (*nil*, message, code) on failure.

	Added: 4.20

	Example:
	```
	rpm.spawn({'systemctl', 'restart', 'httpd'}, {stderr='/dev/null'})
	```

*undefine(*_NAME_*)*
	Undefine a macro. See also *MACROS*.

	Note that this is only pops the most recent macro definition by the
	given name from stack, ie there may be still macro definitions by the
	same name after an undefine operation.

	Example:
	```
	rpm.undefine('zzz')
	```


*vercmp(*_V1_, _V2_*)*
	Perform RPM version comparison on argument strings.
	Returns -1, 0 or 1 if _V1_ is smaller, equal or larger than _V2_.
	See *rpm-version*(7).

	Note: in RPM < 4.16 this operated on version segments only, which
	does not produce correct results on full _EVR_ strings.

	Example:
	```
	rpm.vercmp('1.2-1', '2.0-1')
	```

*ver(*_EVR_*)*, *ver(*_E_, _V_, _R_*)*
	Create RPM version object.
	This takes either an _EVR_ string which is parsed to it's components,
	or epoch, version and release in separate arguments (which can be either
	strings or numbers). The object has three attributes: *e* for epoch,
	*v* for version and *r* for release, can be printed in it's EVR form and
	supports native comparison in Lua.

	Added: 4.17.0

	Example:
	```
	v1 = rpm.ver('5:1.0-2)
	v2 = rpm.ver(3, '5a', 1)
	if v1 < v2 then
	   ...
	end

	if v1.e then
	   ...
	end
	```

## posix extension

Lua standard library offers fairly limited set of io operations.
The *posix* extension greatly enhances what can be done from Lua.

The following functions are available in the *posix* namespace, ie to call
them use *posix.function()*. This documentation concentrates on the Lua
API conventions, for further information on the corresponding system
calls refer to the system manual, eg *access*(3) for *posix.access()*.

*access(*_PATH_ [, _MODE_]*)*
	Test accessibility of file/directory _PATH_. See *access*(3).
	If _MODE_ is omitted then existence is tested, otherwise it is
	a combination of the following tests:
	- *r*: readable
	- *w*: writable
	- *x*: executable
	- *f*: exists

	Example:
	```
	if posix.access('/bin/rpm', 'x') then
	    ...
	end
	```

*chdir(*_PATH_*)*
	Change current working directory to _PATH_. See *chdir*(1).

	Example:
	```
	posix.chdir('/tmp')
	```

*chmod(*_PATH_, _MODE_*)*
	Change file/directory mode. Mode can be either an octal number as for
	*chmod*(2) system call, or a string presentation similar to *chmod*(1).

	Example:
	```
	posix.chmod('aa', 600)
	posix.chmod('bb', 'rw-')
	posix.chmod('cc', 'u+x')
	```

*chown(*_PATH_, _USER_, _GROUP_*)*
	Change file/directory owner/group of _PATH_. The _USER_ and _GROUP_
	arguments may be either numeric id values or user/groupnames.
	See *chown*(2) and *chown*(1).

	Note: This is a privileged operation.

	Example:
	```
	posix.chown('aa', 0, 0)
	posix.chown('bb', 'nobody', 'nobody')
	```

*ctermid()*
	Get controlling terminal name. See *ctermid*(3).

	Example:
	```
	print(posix.ctermid())
	```

*dir(*[_PATH_]*)*
	Get directory contents - like *readdir*(3). If _PATH_ is omitted,
	current directory is used.

	Example:
	```
	for i,p in pairs(posix.dir('/')) do
	    print(p..'\n')
	end
	```

*errno()*
	Get *strerror*(3) message and the corresponding number for current
	*errno*(3).

	Example:
	```
	f = '/zzz'
	if not posix.chmod(f, 100) then
	    s, n = posix.errno()
	    print(f, s)
	end
	```

*exec(*_PATH_ [, _ARGS_...]*)* (OBSOLETE)
	Execute a program. This may only be performed after posix.fork().

	This function is obsolete and only available for RPM v4 packages
	for backwards compatibility.
	Use *rpm.spawn()* or *rpm.execute()* instead.

*files(*[_PATH_]*)*
	Iterate over directory contents. If path is omitted, current directory
	is used.

	Example:
	```
	for f in posix.files('/') do
	    print(f..'\n')
	end
	```

*fork()* (OBSOLETE)
	Fork a new process. See *fork*(2).

	This function is obsolete and only available for RPM v4 packages
	for backwards compatibility.
	Use *rpm.spawn()* or *rpm.execute()* instead.

	Example:
	```
	pid = posix.fork()
	if pid == 0 then
	    posix.exec('/foo/bar')
	elseif pid > 0 then
	    posix.wait(pid)
	end
	```

*getcwd()*
	Get current directory. See *getcwd*(3).

	Example:
	```
	if posix.getcwd() ~= '/' then
	    ...
	endif
	```

*getenv(*_NAME_*)*
	Get an environment variable. See *getenv*(3).

	Example:
	```
	if posix.getenv('HOME') ~= posix.getcwd() then
	    print('not at home')
	end
	```

*getgroup(*_GROUP_*)*
	Get *group*(5) information for a group. _GROUP_ may be either a
	numeric id or group name. If omitted, current group is used.
	Returns a table with fields *name* and *gid* set to group
	name and id respectively, and indexes from 1 onwards specifying group
	members.

	Example:
	```
	print(posix.getgroup('wheel').gid)
	```

*getlogin()*
	Get login name. See *getlogin*(3).

	Example:
	```
	n = posix.getlogin()
	```

*getpasswd*([_USER_ [, _SELECTOR_]]*)*
	Get *passwd*(5) information for a user account. _USER_ may be either a
	numeric id or username. If omitted, current user is used.
	The optional _SELECTOR_ argument may be one of:
	- *name*
	- *uid*
	- *gid*
	- *dir*
	- *shell*
	- *gecos*
	- *passwd*

	If omitted, a table with all these fields is returned.

	Example:
	```
	pw = posix.getpasswd(posix.getlogin(), 'shell')|
	```

*getprocessid(*[_SELECTOR_]*)*
	Get information about current process. The optional _SELECTOR_
	argument may be one of
	- *egid*: effective group id
	- *euid*: effective user id
	- *gid*: group id
	- *uid*: user id
	- *pgrp*: parent group id
	- *pid*: process id
	- *ppid*: parent pid

	If omitted, a table with all these fields is returned.

	Example:
	```
	if posix.getprocessid('pid') == 1 then
	    ...
	end
	```

*kill(*_PID_ [, _SIGNAL_]*)*
	Send a *signal*(7) to a process. _SIGNAL_ must be a numeric value,
	eg. *9* for *SIGKILL*.  If omitted, *SIGTERM* is used.
	See also *kill*(2).

	Example:
	```
	posix.kill(posix.getprocessid('pid'))
	```

*link(*_OLDPATH_, _NEWPATH_*)*
	Create a new name at _NEWPATH_ for a file at _OLDPATH_, aka hard link.
	See also *link*(2).

	Example:
	```
	f = rpm.open('aaa', 'w')
	posix.link('aaa', 'bbb')
	```

*mkdir(*_PATH_*)*
	Create a new directory at _PATH_. See also *mkdir*(2).

	Example:
	```
	posix.mkdir('/tmp')
	```

*mkfifo(*_PATH_*)*
	Create a FIFO aka named pipe at _PATH_. See also *mkfifo*(2).

	Example:
	```
	posix.mkfifo('/tmp/badplace')
	```

*pathconf(*_PATH_ [, _SELECTOR_]*)*
	Get *pathconf*(3) information for _PATH_.
	The optional _SELECTOR_ may be one of
	- *link_max*
	- *max_canon*
	- *max_input*
	- *name_max*
	- *path_max*
	- *pipe_buf*
	- *chown_restricted*
	- *no_trunc*
	- *vdisable*.

	If omitted, a table with all these fields is returned.

	Example:
	```
	posix.pathconf('/', 'path_max')
	```

*putenv(*_STRING_*)*
	Change or add an environment variable. See also *putenv*(3).

	Example:
	```
	posix.putenv('HOME=/me')
	```

*readlink(*_PATH_*)*
	Read value of the symbolic link at _PATH_. See also *readlink*(2).

	Example:
	```
	posix.mkdir('aaa')
	posix.symlink('aaa', 'bbb')
	print(posix.readlink('bbb'))
	```

*rmdir(*_PATH_*)*
	Remove a directory _PATH_. See also *rmdir*(2).

	Example:
	```
	posix.rmdir('/tmp')
	```

*setgid(*_GROUP_*)*
	Set group identity. _GROUP_ may be specified either as a numeric id or
	group name. See also *setgid*(2).

	Note: This is a privileged operation.

*setuid(*_USER_*)*
	Set user identity. _USER_ may be specified either as a numeric id or
	username. See also *setuid*(2).

	Note: This is a privileged operation.

	Example:
	```
	posix.setuid('nobody')
	```

*sleep(*_SECONDS_*)*
	Sleep for the duration of _SECONDS_. See also *sleep*(3).

	Example:
	```
	posix.sleep(5)
	```

*stat(*_PATH_ [, _SELECTOR_]*)*
	Get file *stat*(3) information about a file at _PATH_.
	The optional _SELECTOR_ may be one of
	- *mode*
	- *ino*
	- *dev*
	- *nlink*
	- *uid*
	- *gid*
	- *size*
	- *atime*
	- *mtime*
	- *ctime*
	- *type*.

	If omitted, a table with all these fields is returned.

	Example:
	```
	print(posix.stat('/tmp', 'mode'))|

	s1 = posix.stat('f1')
	s2 = posix.stat('f2')
	if s1.ino == s2.ino and s1.dev == s2.dev then
	    ...
	end
	```

*symlink(*_OLDPATH_, _NEWPATH_*)*
	Create a symbolic link at _NEWPATH_ to _OLDPATH_.
	See also *symlink*(2).

	Example:
	```
	posix.mkdir('aaa')
	posix.symlink('aaa', 'bbb')
	```

*sysconf(*[_SELECTOR_]*)*
	Get *sysconf*(3) information. The optional _SELECTOR_ argument
	may be one of:
	- *arg_max*
	- *child_max*
	- *clk_tck*
	- *ngroups_max*
	- *stream_max*
	- *tzname_max*
	- *open_max*
	- *job_control*
	- *saved_ids*
	- *version*.

	If omitted, a table with all these fields is returned.

	Example:
	```
	posix.sysconf('open_max')|
	```

*times(*[_SELECTOR_]*)*
	Get process and waited-for child process *times*(2).
	The optional _SELECTOR_ argument may be one of
	- *utime*
	- *stime*
	- *cutime*
	- *cstime*
	- *elapsed*

	If omitted, a table with all these fields is returned.

	Example:
	```
	t = posix.times()
	print(t.utime, t.stime)
	```

*ttyname(*[_FD_]*)*
	Get name of a terminal associated with file descriptor _FD_.
	If _FD_ is omitted, *0* (aka standard input) is used. See *ttyname*(3).

	Example:
	```
	if not posix.ttyname() then
	    ...
	endif
	```

*umask(*[_MODE_]*)*
	Get or set process *umask*(2). _MODE_ may be specified as an octal
	number or mode string similarly to *posix.chmod()*.

	Example:
	```
	print(posix.umask())
	posix.umask(222)
	posix.umask('ug-w')
	posix.umask('rw-rw-r--')
	```

*uname(*_FORMAT_*)*
	Get *uname*(2) information about the current system.
	The following format directives are supported:

	- *%m*:  Name of the hardware type
	- *%n*:  Name of this node
	- *%r*:  Current release level of this implementation
	- *%s*:  Name of this operation system
	- *%v*:  Current version level of this implementation

	Example:
	```
	print(posix.uname('%s %r'))
	```

*utime(*_PATH_ [, _MTIME_ [, _CTIME_]]*)*
	Change last access and modification times.
	mtime and ctime are expressed seconds since epoch. See *utime*(2).

	If _MTIME_ or _CTIME_ are omitted, current time is used, similar
	to *touch*(1).

	Example:
	```
	posix.mkdir('aaa')
	posix.utime('aaa', 0, 0)
	```

*wait(*[_PID_]*)* (DEPRECATED)
	Wait for a child process. If _PID_ is specified wait for that
	particular child. See also *wait*(2).

	This function is obsolete and only available for RPM v4 packages
	for backwards compatibility.
	Use *rpm.spawn()* or *rpm.execute()* instead.

	Example:
	```
	pid = posix.fork()
	if pid == 0 then
	    posix.exec('/bin/ls'))
	elseif pid > 0 then
	    posix.wait(pid)
	end
	```


*setenv(*_NAME_, _VALUE_ [, _OVERWRITE_]*)*
	Change or add environment variable _NAME_. The optional _OVERWRITE_
	is a boolean which defines behavior when a variable by the same name
	already exists. See also *setenv*(3).

	Example:
	```
	posix.setenv('HOME', '/me', true)
	```

*unsetenv(*_NAME_*)*
	Remove a variable _NAME_ from environment. See also *unsetenv*(3).

	Example:
	```
	posix.unsetenv('HOME')
	```

# EXTENDING AND CUSTOMIZING
On initialization, RPM executes a global _init.lua_ Lua initialization
script from the directory *%getconfdir* expands to, typically
_/usr/lib/rpm/init.lua_. This can be used to customize the rpm Lua
environment without recompiling RPM.

For the embedded Lua interpreter, module's loaded with *require* are
primarily searched from *%{getconfdir}/lua/*. *%\_rpmluadir* is a shorthand
for this path.

# SEE ALSO
*rpm-macros*(7) *rpm-payloadflags*(7) *rpmlua*(1) *rpm-version*(7)

*https://www.lua.org/*
