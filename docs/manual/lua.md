---
layout: default
title: rpm.org - Embedded Lua interpreter
---
# Lua in RPM

A fairly unknown feature of RPM is that it comes with an embedded Lua interpreter. This page attempts to document the possibilities of the embedded Lua. Note that Lua-support is a compile-time option, the following assumes that your RPM is was built using --with-lua option.

## Lua scriptlets

The internal Lua can be used as the interpreter of rpm any scriptlets (%pre, %post etc):

```
%pre -p <lua>
print("Hello from Lua")
```

The point? Remember, Lua is embedded in rpm. This means the Lua scriptlets run in the rpm process context, instead of forking a new process to execute something. This has a number of advantages over, say, using /bin/sh as scriptlet interpreter:

* No forking involved means it runs faster. Lua itself is fairly fast as an interpreter too.
* No external dependencies introduced to packages.
* The internal interpreter can run when there's nothing at all installed yet, because it doesn't need to be exec()'ed. Consider the initial install phase: before even /bin/sh is available to execute the simplest shell built-in commands, the shell's dependencies will have to be installed. What if some of those need scriptlets?
    Internal Lua is the only thing that can reliably run in %pretrans. On initial system installation, there's absolutely nothing in the environment where %pretrans scriptlets execute. This is a condition you cannot even detect with any other means: testing for existence of a file or directory would otherwise require a shell, which is not there.
 * Syntax errors in <lua> scripts are detected at package build time.
 * As it runs in within the rpm process context, it can do things that external process cannot do, such as define new macros. 

Scriptlet arguments are accessible from a global 'arg' table. Note: in Lua, indexes customarily start at 1 (one) instead of 0 (zero), and for the better or worse, the rpm implementation follows this practise. Thus the scriptlet arg indexes are off by one from general expectation based on traditional scriptlet arguments. The argument containing number of installed package instances is arg[2] and the similar argument for trigger targets is arg[3], whereas traditionally they are 1 and 2 (eg $1 and $2 in shell scripts).

While scriptlets shouldn't be allowed to fail normally, you can signal scriptlet failure status by using Lua's error(msg, [level]) function if you need to. As <lua> scriptlets run within the rpm process itself, care needs to be taken within the scripts - eg os.exit() must not be called (see ticket #167). In newer rpm versions (>= 4.9.0) this is not an issue as rpm protects itself by returning errors on unsafe os.exit() and posix.exec().

## Lua macros

The internal Lua interpreter can be used for dynamic macro content creation:

```
%{lua: print("Requires: foo >= 1.2")}
```

The above is a silly example and doesn't even begin to show how powerful a feature this is. For a slightly more complex example, RPM itself uses this to implement %patches and %sources macros (new in RPM 4.6.0):

```
%patches %{lua: for i, p in ipairs(patches) do print(p.." ") end}
%sources %{lua: for i, s in ipairs(sources) do print(s.." ") end}
```

Parametric Lua macros receive their options and arguments as two local
tables "opt" and "arg", where "opt" holds processed option values keyed by
the option character, and "arg" contains arguments numerically indexed.
These tables are always present regardless of whether options or arguments
were actually passed to simplify use. (rpm >= 4.17)

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
and assignment can be used to define or undefine macros. (rpm >= 4.17)

Parametric macros (including all built-in macros) can be called in a Lua
native manner via the `macros` table. The argument can be either a
single string (`macros.with('thing')`), in which case it's expanded
and split with the macro-native rules, or it can be a table
`macros.dostuff({'one', 'two', 'three'})` in which case the table contents
are used as literal arguments that are not expanded in any way. (rpm >= 4.17)

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
rpm.define("foo 1")
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

#### expand(arg)

Perform rpm macro expansion on argument string.

```
rpm.expand("/usr%{__lib}/mypath")
rpm.expand("%{_libdir}")
```

#### interactive()

Launch interactive session for testing and debugging.

```
rpm --eval "%{lua: rpm.interactive()}"
```

#### isdefined(name)

Test whether a macro is defined and whether it's parametric, returned in two booleans (rpm >= 4.17.0)

```
if rpm.isdefined("_libdir") then
    ...
end
```

#### load(path)

Load a macro file from given path (same as built-in `%{load:...}` macro)

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

#### redirect2null(fdno)

Redirect file descriptor fdno to /dev/null (rpm >= 4.16, in rpm 4.13-4.15
this was known as posix.redirect2null())

```
pid = posix.fork()
if pid == 0 then
    posix.redirect2null(2)
    assert(posix.exec("/bin/awk"))
elseif pid > 0 then
    posix.wait(pid)
end

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
rpm.vercmp("1.2-1", "2.0-1")
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

Lua standard library offers fairly limited set of io operations. The posix extension greatly enhances what can be done from Lua. The following functions are available in "posix" namespace, ie to call them use posix.function().

| Function  |  Explanation  |   Example |
|-----------|---------------|-----------|
|access(path, [mode])   | Test file/directory accessibility  | if posix.access("/bin/rpm", "x") then ... end|
|chdir(path)   |  Change directory  |  posix.chdir("/tmp")
|chmod(path, modestring)   |  Change file/directory mode   |
|chown(path, uid, gid)  | Change file/directory owner/group    |
|ctermid()         |
|dir([path])   |  Get directory contents - like readdir()   |  for i,p in pairs(posix.dir("/tmp")) do print(p.."\n") end|
|errno()   |  Get errno value and message print(posix.errno())|
|exec(path, [args...])  | Exec a program  |
|files([path])  | Iterate over directory contents   |  for f in posix.files("/tmp") do print(f..'\n') end|
|fork()  |Fork a new process  |
|getcwd()   | Get current directory   |
|getenv(name)  |  Get environment variable    |
|getgroup(gid)  | Get group members (gid is number or string)     |
|getlogin() | Get login name  |
|getpasswd(uid, selector)  |  Get passwd information (username, uid, gid, shell, gecos...)  |  posix.getpasswd(posix.getlogin(), "shell")|
|getprocessid(selector) | Get process ID information (gid, uid, pid, ppid...)   |  posix.getprocessid("pid")|
|kill(pid, signal)  | Send a signal to a process  |
|link(oldpath, newpath)  |Create a hard link  |
|mkdir(path)   |  Create a new directory  |
|mkfifo(path)   | Create a FIFO   |
|pathconf() | Get pathconf values     |
|putenv()   | Change or add an environment variable   |
|readlink(path) | Read symlink value  |
|rmdir(path)   |  Remove a directory  |posix.rmdir("/tmp")|
|setgid(gid)   |  Set group identity  |
|setuid(uid)   |  Set user identity   |
|sleep(seconds) | Sleep for specified number of seconds  | posix.sleep(5)|
|stat(path, selector)  |  Perform stat() on a file/directory (mode, ino, dev, nlink, uid, gid...) |    posix.stat("/tmp", "mode")|
|symlink(oldpath, newpath)  | Create a symlink    |
|sysconf(name)  | Access sysconf values  | posix.sysconf("open_max")|
|times()   |  Get process times   |
|ttyname() |  Get current tty name    |
|umask()   |  Get current umask   |
|uname([format])   |  Get information about current kernel  |  posix.uname("%r")|
|utime()   |  Change access/modification time of an inode     |
|wait() | Wait for a child process    |
|setenv()   | Change or add an environment variable   |
|unsetenv() | Remove a variable from environment  |

### rex extension

This extension adds regular expression matching to Lua.

A simple example:
```
expr = rex.new(<regex<)
if expr:match(<arg<) then
    ... do stuff ...
end
```

TODO: fully document

## Extending and customizing

On initialization, RPM executes a global init.lua Lua initialization script, typically located in /usr/lib/rpm/init.lua. This can be used for performing custom runtime configuration of RPM and adding global functions and variables to the RPM Lua environment without recompiling rpm.

