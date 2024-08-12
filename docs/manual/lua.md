---
layout: default
title: rpm.org - Embedded Lua interpreter
---
# Lua in RPM

A fairly unknown feature of RPM is that it comes with an embedded Lua interpreter. This page attempts to document the possibilities of the embedded Lua. Note that Lua-support is a compile-time option, the following assumes that your RPM is was built using --with-lua option.

For testing and debugging you can run the RPM Lua interpreter in standalone
mode with the rpmlua(8) command.

## Lua scriptlets

The internal Lua can be used as the interpreter of rpm any scriptlets (%pre, %post etc):

```
%pre -p <lua>
print('Hello from Lua')
```

The point? Remember, Lua is embedded in rpm. This means the Lua scriptlets run in the rpm process context, instead of forking a new process to execute something. This has a number of advantages over, say, using /bin/sh as scriptlet interpreter:

* No forking involved means it runs faster. Lua itself is fairly fast as an interpreter too.
* No external dependencies introduced to packages.
* The internal interpreter can run when there's nothing at all installed yet, because it doesn't need to be exec()'ed. Consider the initial install phase: before even /bin/sh is available to execute the simplest shell built-in commands, the shell's dependencies will have to be installed. What if some of those need scriptlets?
    Internal Lua is the only thing that can reliably run in %pretrans. On initial system installation, there's absolutely nothing in the environment where %pretrans scriptlets execute. This is a condition you cannot even detect with any other means: testing for existence of a file or directory would otherwise require a shell, which is not there.
 * Syntax errors in <lua> scripts are detected at package build time.
 * As it runs in within the rpm process context, it can do things that external process cannot do, such as define new macros. 

Scriptlet arguments are accessible from a global 'arg' table. Note: in Lua, indexes customarily start at 1 (one) instead of 0 (zero), and for the better or worse, the rpm implementation follows this practise. Thus the scriptlet arg indexes are off by one from general expectation based on traditional scriptlet arguments. The argument containing number of installed package instances is arg[2] and the similar argument for trigger targets is arg[3], whereas traditionally they are 1 and 2 (eg $1 and $2 in shell scripts).

Scriptlets of relocatable packages additionally carry a global
`RPM_INSTALL_PREFIX` table containing all the possible prefixes of the
package. (rpm >= 4.18.0)

While scriptlets shouldn't be allowed to fail normally, you can signal scriptlet failure status by using Lua's error(msg, [level]) function if you need to. As <lua> scriptlets run within the rpm process itself, care needs to be taken within the scripts - eg os.exit() must not be called (see ticket #167). In newer rpm versions (>= 4.9.0) this is not an issue as rpm protects itself by returning errors on unsafe os.exit() and posix.exec().

## Lua macros

The internal Lua interpreter can be used for dynamic macro content creation:

```
%{lua: print('Requires: foo >= 1.2')}
```

The above is a silly example and doesn't even begin to show how powerful a feature this is. For a slightly more complex example, RPM itself uses this to implement %patches and %sources macros (new in RPM 4.6.0):

```
%patches %{lua: for i, p in ipairs(patches) do print(p..' ') end}
%sources %{lua: for i, s in ipairs(sources) do print(s..' ') end}
```

## Macro integration (rpm >= 4.17)

Parametric Lua macros receive their options and arguments as two local
tables `opt` and `arg`, where `opt` holds processed option values keyed by
the option character, and `arg` contains arguments numerically indexed.
These tables are always present regardless of whether options or arguments
were actually passed to simplify use.

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

Macros can be accessed via a global `macros` table in the Lua environment.
Lua makes no difference between index and field name syntax so
`macros.foo` and `macros['foo']` are equivalent, use what better suits the
purpose. Like any real Lua table, non-existent items are returned as `nil`,
and assignment can be used to define or undefine macros.

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

Parametric macros (including all built-in macros) can be called in a Lua
native manner via the `macros` table, with either macros.name or macros[name]
syntax. The argument can be either a single string (`macros.with('thing')`),
in which case it's expanded and split with the macro-native rules,
or it can be a table `macros.dostuff({'one', 'two', 'three'})` in which
case the table contents are used as literal arguments that are not expanded
in any way.

Lua macros can also also `return` their output, which makes programming
helper macros look more natural. For example:

```
%sum() %{lua:
   local v = 0
   for _, a in ipairs(arg) do
       v = v + tonumber(a)
   end
   return v
}
```

## Available Lua extensions in RPM

In addition to all Lua standard libraries (subject to the Lua version rpm is linked to), a few custom extensions are available in the RPM internal Lua interpreter. These can be used in all contexts where the internal Lua can be used.

### rpm extension

The following RPM specific functions are available:

#### b64decode(arg)

Perform base64 decoding on argument (rpm >= 4.8.0)

```
blob = 'binary data'
print(blob)
e = rpm.b64encode(blob)
print(e)
d = rpm.b64decode(e)
print(d)
```

#### b64encode(arg [, linelen])

Perform base64 encoding on argument (rpm >= 4.8.0)
Line length may be optionally specified via second argument.

See `b64decode()`

#### define(name, body)

Define a global macro `name` with `body`.
Note that macros are stacked by name, so this is actually a push operation
and any previous definitions of the same name remain underneath after
a define.

```
rpm.define('foo 1')
```

See also `undefine()`

#### execute(path [, arg1 [,...])

Execute an external command (rpm >= 4.15.0)
This is handy for executing external helper commands without depending
on the shell. The first argument is the command to execute, followed
by optional number of arguments to pass to the command.

```
rpm.execute('ls', '-l', '/')
```

For better control over the process execution and output, see rpm.spawn().

#### expand(arg)

Perform rpm macro expansion on argument string.

```
rpm.expand('/usr%{__lib}/mypath')
rpm.expand('%{_libdir}')
```

#### glob(pattern, [flags])

Return pathnames matching pattern in a table.
If flags contains "c", return the original pattern in case of no matches.

```
for i, p in ipairs(rpm.glob('*')) do
    print(p)
end
```

#### interactive()

Launch interactive session for testing and debugging.

```
rpm --eval "%{lua: rpm.interactive()}"
```

#### isdefined(name)

Test whether a macro is defined and whether it's parametric, returned in two booleans (rpm >= 4.17.0)

```
if rpm.isdefined('_libdir') then
    ...
end
```

#### load(path)

Load a macro file from given path (same as built-in `%{load:...}` macro)

```
rpm.load('my.macros')
```

#### open(path, [mode])

Open a file stream using rpm IO facilities, with support for transparent
compression and decompression (rpm >= 4.17.0). Path is file name string,
optionally followed with mode string to specify open behavior and possible
compression in the form flags[.io] where the flags may be a combination of
the following (but not all combinations are legal, and not all IO types
support all flags):

| Flag | Explanation
|-------------------
| a | Open for append
| w | Open for writing, truncate
| r | Open for reading
| + | Open for reading and writing
| x | Fail if file exists
| T | Enable thread support (xzdio and zstdio)
| ? | Enable IO debugging

and optionally followed by IO type (aka compression) to use:

| IO     | Explanation
|---------------------
| bzdio  | BZ2 compression
| fdio   | Uncompressed IO (without URI parsing)
| gzdio  | ZLIB and GZ compression
| ufdio  | Uncompressed IO (default)
| xzdio  | XZ compression
| zstdio | ZSTD compression

Read and print a gz compressed file:
```
f = rpm.open('some.txt.gz', 'r.gzdio')
print(f:read())
```

The returned rpm.fd object has the following methods:

##### close()

Close the file stream.

```
f = rpm.open('file')
f:close()
```

##### flush()

Flush the file stream.

```
f = rpm.open('file', 'w')
f:write('foo')
f:flush()
f:close()
```

##### read([len])

Read data from the file stream up to `len` bytes or if not specified,
the entire file.

```
f = rpm.open('/some/file')
print(f:read())
```

##### seek(mode, offset)

Reposition the file offset of the stream. Mode is one of `set`, `cur`
and `end`, and offset is relative to the mode: absolute, relative to
current or relative to end. Not all streams support seeking.

Returns file offset after the operation.

See also `man lseek`

```
f = rpm.open('newfile', 'w')
f:seek('set', 555)
f:close()
```

##### write(buf [, len])

Write data in `buf` to the file stream, either in its entirety or up
to `len` bytes if specified.

```
f = rpm.open('newfile', 'w')
f:write('data data')
f:close()
```

##### reopen(mode)

Reopen a stream with a new mode (see `rpm.open()`).

```
rpm.open('some.txt.gz')
f = f:reopen('r.gzdio')
print(f:read())}
```

#### redirect2null(fdno) (DEPRECATED)

Redirect file descriptor fdno to /dev/null (rpm >= 4.16, in rpm 4.13-4.15
this was known as posix.redirect2null())

```
pid = posix.fork()
if pid == 0 then
    posix.redirect2null(2)
    assert(posix.exec('/bin/awk'))
elseif pid > 0 then
    posix.wait(pid)
end
```

This function is deprecated and scheduled for removal in 6.0,
use `rpm.spawn()` or `rpm.execute()` instead.

#### spawn({command} [, {actions}])

Spawn, aka execute, an external program. (rpm >= 4.20)

`{command}` is a table consisting of the command and its arguments.
An optional second table can be used to pass various actions related
to the command execution, currently supported are:

| Action  | Argument(s) | Description
|---------|----------------------
| `stdin` | path        | Redirect standard input to path
| `stdout`| path        | Redirect standard output to path
| `stderr`| path        | Redirect standard error to path

Returns the command exit status: zero on success, or a tuplet
of (nil, message, code) on failure.

```
rpm.spawn({'systemctl', 'restart', 'httpd'}, {stderr='/dev/null'})
```

#### undefine(name)

Undefine a macro (rpm >= 4.14.0)
Note that this is only pops the most recent macro definition by the
given name from stack, ie there may be still macro definitions by the
same name after an undefine operation.

```
rpm.undefine('zzz')
```

See also `define()`

#### vercmp(v1, v2)

Perform RPM version comparison on argument strings (rpm >= 4.7.0).
Returns -1, 0 or 1 if `v1` is smaller, equal or larger than `v2`.

```
rpm.vercmp('1.2-1', '2.0-1')
```

Note that in rpm < 4.16 this operated on version segments only, which
does not produce correct results on full EVR strings.

#### ver(evr), ver(e, v, r)

Create rpm version object (rpm >= 4.17.0)
This takes either an EVR string which is parsed to it's components,
or epoch, version and release in separate arguments (which can be either
strings or numbers). The object has three attributes: `e` for epoch,
`v` for version and `r` for release, can be printed in it's EVR form and
supports native comparison in Lua:

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

### posix extension

Lua standard library offers fairly limited set of io operations.
The posix extension greatly enhances what can be done from Lua.
The following functions are available in `posix` namespace, ie to call
them use posix.function(). This documentation concentrates on the Lua
API conventions, for further information on the corresponding system
calls refer to the system manual, eg `man 3 access` for `posix.access()`.
Many but not all functions have also system utility counterparts which
may more closely resemble the Lua API, eg `man 1 chmod`.

#### access(path [, mode])
Test file/directory accessibility. If mode is omitted then existence is
tested, otherwise it is a combination of the following tests:

| Flag | Explanation
|---------------------
| r    | Readable
| w    | Writable
| x    | Executable
| f    | Existence

```
if posix.access('/bin/rpm', 'x') then
    ...
end
```

#### chdir(path)

Change current working directory.

```
posix.chdir('/tmp')
```

#### chmod(path, mode)

Change file/directory mode. Mode can be either an octal number as for 
chmod() system call, or a string presenation similar to chmod utility.

```
posix.chmod('aa', 600)
posix.chmod('bb', 'rw-')
posix.chmod('cc', 'u+x')
```

#### chown(path, user, group)

Change file/directory owner/group. The user and group may be either numeric
id values or user/groupnames. This is a privileged operation.

```
posix.chown('aa', 0, 0)
posix.chown('bb', 'nobody', 'nobody')
```

#### ctermid()

Get controlling terminal name.

```
print(posix.ctermid())
```

#### dir([path])

Get directory contents - like readdir(). If path is omitted, current
directory is used.

```
for i,p in pairs(posix.dir('/')) do
    print(p..'\n')
end
```

#### errno()
Get strerror() message and the corresponding number for current `errno`.

```
f = '/zzz'
if not posix.chmod(f, 100) then
    s, n = posix.errno()
    print(f, s)
end
```

#### exec(path [, args...]) (DEPRECATED)

Execute a program. This may only be performed after posix.fork().

This function is deprecated and scheduled for removal in 6.0,
use `rpm.spawn()` or `rpm.execute()` instead.

#### files([path])

Iterate over directory contents. If path is omitted, current directory
is used.

```
for f in posix.files('/') do
    print(f..'\n')
end
```

#### fork() (DEPRECATED)

Fork a new process. 

This function is deprecated and scheduled for removal in 6.0,
use `rpm.spawn()` or `rpm.execute()` instead.

```
pid = posix.fork()
if pid == 0 then
    posix.exec('/foo/bar')
elseif pid > 0 then
    posix.wait(pid)
end
```

#### getcwd()

Get current directory.

```
if posix.getcwd() ~= '/' then
    ...
endif
```

#### getenv(name)

Get environment variable

```
if posix.getenv('HOME') ~= posix.getcwd() then
    print('not at home')
end
```

#### getgroup(group)

Get information about a group. The group may be either a numeric id or
group name. Returns a table with fields `name` and `gid` set to group
name and id respectively, and indexes from 1 onwards specifying group
members.

```
print(posix.getgroup('wheel').gid)
```

#### getlogin()

Get login name .

```
n = posix.getlogin()
```

#### getpasswd([user [, selector]])

Get passwd information for a user account. User may be either a numeric id
or user name (if nil, current user is used). The optional selector may be
one of `name`, `uid`, `gid`, `dir`, `shell`, `gecos` and `passwd` and if
omitted, a table with all these fields is returned.

```
pw = posix.getpasswd(posix.getlogin(), 'shell')|
```

#### getprocessid([selector])

Get information about current process. The optional selector may be one of
`egid`, `euid`, `gid`, `uid`, `pgrp`, `pid` and `ppid` and if omitted,
a table with all these fields is returned.

```
if posix.getprocessid('pid') == 1 then
    ...
end
```

#### kill(pid [, signal])

Send a signal to a process. Signal must be a numeric value, eg 9 for SIGKILL.
If omitted, SIGTERM is used.

```
posix.kill(posix.getprocessid('pid'))
```

#### link(oldpath, newpath)

Create a new name for a file, aka hard link.

```
f = rpm.open('aaa', 'w')
posix.link('aaa', 'bbb')
```

#### mkdir(path)

Create a new directory.

```
posix.mkdir('/tmp')
```

#### mkfifo(path)

Create a FIFO aka named pipe. 

```
posix.mkfifo('/tmp/badplace')
```

#### pathconf(path [, selector])

Get `pathconf(3)` information. The optional selector may be one of
`link_max`, `max_canon`, `max_input`, `name_max`, `path_max`, `pipe_buf`,
`chown_restricted`, `no_trunc` and `vdisable`, and if omitted, a table
with all these fields is returned.

```
posix.pathconf('/', 'path_max')
```

#### putenv(arg)

Change or add an environment variable.

```
posix.putenv('HOME=/me')
```

#### readlink(path)

Read symlink value

```
posix.mkdir('aaa')
posix.symlink('aaa', 'bbb')
print(posix.readlink('bbb'))
```

#### rmdir(path)

Remove a directory

```
posix.rmdir('/tmp')
```

#### setgid(group)

Set group identity. Group may be specified either as a numeric id or
group name. This is a privileged operation.

#### setuid(user)

Set user identity. Use may be specified either as a numeric id or
user name. This is a privileged operation.

```
posix.setuid('nobody')
```

#### sleep(seconds)

Sleep for specified number of seconds

```
posix.sleep(5)
```

#### stat(path [, selector])

Get information about a file at path.
The optional selector may be one of `mode`, `ino`, `dev`, `nlink`, `uid`,
`gid`, `size`, `atime`, `mtime`, `ctime` and `type`, or if omitted a
table with all these fields is returned.

```
print(posix.stat('/tmp', 'mode'))|

s1 = posix.stat('f1')
s2 = posix.stat('f2')
if s1.ino == s2.ino and s1.dev == s2.dev then
    ...
end
```

#### symlink(oldpath, newpath)

Create a symbolic link to a path.

```
posix.mkdir('aaa')
posix.symlink('aaa', 'bbb')
```

#### sysconf(name [, selector])

Get `sysconf(3)` information. The optional selector may be one of 
`arg_max`, `child_max`, `clk_tck`, `ngroups_max`, `stream_max`, `tzname_max`,
`open_max`, `job_control`, `saved_ids` and `version`. If omitted, a table
with all these fields is returned.

```
posix.sysconf('open_max')|
```

#### times([selector])

Get process and waited-for child process times. The optional selector may
be one of `utime`, `stime`, `cutime`, `cstime` and `elapsed`. If omitted,
a table with all these fields is returned.

```
t = posix.times()
print(t.utime, t.stime)
```

#### ttyname([fd])

Get name of a terminal associated with file descriptor fd. If fd is omitted,
0 (aka stdin) is used.

```
if not posix.ttyname() then
    ...
endif
```

#### umask([mode])

Get or set process umask. Mode may be specified as an octal number or
mode string similarly to posix.chmod().

```
print(posix.umask())
posix.umask(222)
posix.umask('ug-w')
posix.umask('rw-rw-r--')
```

#### uname(format)

Get information about current system. The following format directives are
supported:

|Format|Explanation
|-------------------
| %m   | Name of the hardware type
| %n   | Name of this node
| %r   | Current release level of this implementation
| %s   | Name of this operation system
| %v   | Current version level of this implementation

```
print(posix.uname('%r'))
```

#### utime(path [, mtime] [, ctime])

Change file last access and modification times. 
mtime and ctime are expressed seconds since epoch.
If mtime or ctime are omitted, current time is used (similar to `touch(1)`)

```
posix.mkdir('aaa')
posix.utime('aaa', 0, 0)
```

#### wait([pid]) (DEPRECATED)

Wait for a child process. If pid is specified wait for that particular child.

```
pid = posix.fork()
if pid == 0 then
    posix.exec('/bin/ls'))
elseif pid > 0 then
    posix.wait(pid)
end
```

This function is deprecated and scheduled for removal in 6.0,
use `rpm.spawn()` or `rpm.execute()` instead.

#### setenv(name, value [, overwrite])

Change or add an environment variable. The optional overwrite is a boolean
which defines behavior when a variable by the same name already exists.

```
posix.setenv('HOME', '/me', true)
```

#### unsetenv(name)

Remove a variable from environment.

```
posix.unsetenv('HOME')
```

## Extending and customizing

On initialization, RPM executes a global init.lua Lua initialization script, typically located in /usr/lib/rpm/init.lua. This can be used for performing custom runtime configuration of RPM and adding global functions and variables to the RPM Lua environment without recompiling rpm.

