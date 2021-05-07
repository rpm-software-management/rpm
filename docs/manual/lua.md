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

Parametric macros (including all built-in macros) can be called in a Lua
native manner via the `macros` table. The argument can be either a
single string (`macros.with('thing')`), in which case it's expanded
and split with the macro-native rules, or it can be a table
`macros.dostuff({'one', 'two', 'three'})` in which case the table contents
are used as literal arguments that are not expanded in any way.

## Available Lua extensions in RPM

In addition to all Lua standard libraries (subject to the Lua version rpm is linked to), a few custom extensions are available in the RPM internal Lua interpreter. These can be used in all contexts where the internal Lua can be used.

### rpm extension

The following RPM specific functions are available:

| Function | Explanation | Example |
|----------|-------------|---------|
|define(arg) | Define a global macro. | rpm.define("foo 1") |
|isdefined(arg) | Test whether a macro is defined and whether it's parametric, returned in two booleans. | if rpm.isdefined("_libdir") then ... end |
|expand(arg)  |   Perform macro expansion.  |  rpm.expand("%{_libdir}")
|register() | Register an RPM hook    |
|unregister()  |  Unregister an RPM hook  |
|call()  |Call an RPM hook    |
|interactive() |  Launch interactive session (only for testing + debugging) |  rpm --eval "%{lua: rpm.interactive()}"|
|vercmp()   | Perform RPM version comparison (rpm >= 4.7.0)  | rpm.vercmp("1.2-1", "2.0-1")|
|b64encode() |    Perform base64 encoding (rpm >= 4.8.0)  |
|b64decode()  |   Perform base64 decoding (rpm >= 4.8.0)   |

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

