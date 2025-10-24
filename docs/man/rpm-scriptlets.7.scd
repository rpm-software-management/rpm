RPM-SCRIPTLETS(7)

# NAME
rpm-scriptlets - RPM transaction scriptlets

# SYNOPSIS
*%*_SCRIPT_ [options] [_NAME_]++
\[body]

*%*_TRIGGER_ [options] [_NAME_] *--* _TRIGGERCOND_ ...++
\[body]

*%*_FILETRIGGER_ [options] [filetrigger-options] [_NAME_] *--* _PATHPREFIX_ ...++
\[body]

# DESCRIPTION
RPM supports running arbitrary programs at various pre-determined stages
of a package's lifetime, such as after installation, just before uninstall
or when another package is installed or updated in _transactions_.
These programs are called _scriptlets_ and the places where they run are
called _slots_.

There are three main types of scriptlets:
- *Scripts* are unconditionally invoked at their predetermined slots
  in a transaction.
- *Triggers* are conditionally invoked when their specified _trigger condition_
  matches other packages on the installed system or in a transaction.
- *File triggers* are conditionally invoked when their specified _path prefix_
  matches other files on the installed system or in a transaction.

By default, scriptlets are executed with the _/bin/sh_ shell, but
this can be overridden (see *OPTIONS*). Typically a scriptlet has a body for
the interpreter to execute, but the interpreter can also be a standalone,
special-purpose program, in which case the scriptlet body may be omitted.
The scriptlet interpreter dependency is automatically added by RPM, but
other dependencies to tools used by scriptlets must be manually added
in the spec.

Scriptlets cannot be interactive, and should not rely on their output
to be seen by users.

# ARGUMENTS
_NAME_
	The name of the subpackage the scriptlet is associated with, within
	the spec file. If omitted, the main package is assumed.
	See *rpm-spec*(5) for details.

_TRIGGERCOND_
	A trigger condition is a name of a package, optionally followed
	by a version range: _NAME_ [_op_ _EVR_] where _op_ is one of
	*<*, *<=*, *=*, *>=* or *>* and _EVR_ is an *rpm-version*(7) label.
	For example, *glibc < 2.40*.

_PATHPREFIX_
	A path prefix on which the file trigger is triggered on. The prefix
	must be a complete path of its own, typically a directory whose
	contents the trigger is interested in, e.g. _/usr/lib_.

# OPTIONS
*-e*
	Perform runtime *rpm-macros*(7) expansion on the scriptlet before
	executing it.

	Note that macros intended to be runtime expanded generally need
	to be escaped in specs to avoid buildtime expansion.

*-n*
	Interpret _NAME_ as a full package name.

*-p* _EXECUTABLE_
	Execute the scriptlet using _EXECUTABLE_ as the interpreter.

	As a special case, specifying *<lua>* as the _EXECUTABLE_ invokes the
	embedded Lua interpreter. The various special behaviors of such
	scriplets are documented in the *rpm-lua*(7) manual.

*-q*
	Perform *rpm-queryformat*(7) on the package header, using the
	scriptlet as the format.

	Note the need to escape things that look like macros, and constructs
	that are special to the shell, such as *{}* and *[]*.

# FILETRIGGER OPTIONS
*-P* _NUMBER_
	Priority of the trigger, among the other file triggers of the same
	kind. The default priority of file triggers is 100000. A larger
	priority means earlier execution.

# SCRIPTS
*%*_SCRIPT_ [options] [_NAME_]++
\[body]

Scripts execute unconditionally at their pre-determined slots in a transaction.

Scripts receive one argument (*$1*), which contains the number of installed
instances of the package when the operation on the package containing the
executing script completes. RPM performs upgrades as two separate but
related operations: install of the new version, and erase of the old version.
This can be used to differentiate install, erase and upgrade/downgrade
from each other: the argument is *1* on initial install, and *0* on final
removal. Otherwise it's an upgrade/downgrade (or parallel install).

The scripts of a package can be examined with a *rpm -q --scripts* query.

## Package scripts
Package scripts execute before and after the main operation
(install/remove etc.) of the package inside a transaction.

Package scripts should only be used for actions that are fundamentally
package specific. Domain specific registries, databases, caches and such are
much better handled centrally by file triggers. Similarly, user and group
creation should be handled by *rpm-sysusers*(7), rather than scripting them.

*%pre*
	Executed just before unpacking the contents of the package.
	Non-zero exit prevents the installation of the containing package.

*%post*
	Executed just after unpacking the contents of the package.

*%preun*
	Executed just before removing the contents of the package.
	Non-zero exit prevents the uninstallation of the containing package.

*%postun*
	Executed just after removing the contents of the package.

*%verify*
	Executed when a package is verified using *rpm --verify*.

	Unlike all the other scriptlet types, *%verify* never executes as
	a part of install/erase etc. operations.

## Transaction scripts
Transaction scripts run before and after all the other package level
operations (install/remove etc.) in a transaction.

When multiple transaction scripts for a given slot are present in a transaction,
they are executed in the order of their install/removal order within
the transaction.

*%pretrans*
	Executed just before an install/update/reinstall transaction on the
	containing package starts.

	No files from the transaction have been installed or removed yet.
	That is, in a fresh installation to an empty system root, there are no
	files around and no interpreter to run, so the only interpreter
	that can be reliably used in this slot is the embedded *rpm-lua*(7)
	interpreter.

	Non-zero exit prevents the installation of the containing package.

	This is a very special and a dangerous slot, and is best avoided.

*%posttrans*
	Executed just after an install/update/reinstall transaction on the
	containing package finishes.

	All files from the transaction have been installed or removed at
	this point.

*%preuntrans*
	Executed just before an uninstall/updated-from transaction on the
	containing package starts.

*%postuntrans*
	Executed just after an uninstall/updated-from transaction on the
	containing package finishes.

# TRIGGERS
*%*_TRIGGER_ [options] [_NAME_] *--* _TRIGGERCOND_ ...++
\[body]

Triggers are package scriptlets that are set off by changes to other packages.
Unlike scripts, triggers are only executed when their _trigger condition_
is satisfied. Besides just reacting to changes to other packages, they are
often used for version specific migration tasks.

A trigger scriptlet may have more than one condition, and
any of them matching sets off the trigger. A trigger will only run
once even if there are multiple packages setting it off.

Triggers receive one argument (*$1*), which contains the
number of installed instances of the _triggered package_ (ie. the package
containing the trigger scriptlet) when the operation has completed.
In addition, triggers receive a second argument (*$2*), which contains the
number of installed instaces of the _triggering package_ (ie. the package
that set off the trigger).

The triggers of a package can be examined with a *rpm -q --triggers* query.

*%triggerprein*
	Executed just before the installation of a triggering package.

*%triggerin*
	Executed just after the installation of a triggering package.

*%triggerun*
	Executed just before the removal of a triggering package.

*%triggerpostun*
	Executed just after the removal of a triggering package.

# FILE TRIGGERS
*%*_FILETRIGGER_ [options] [filetrigger-options] [_NAME_] *--* _PATHPREFIX_ ...++
\[body]

File triggers are scriptlets that are set off by changes to files, ie.
triggers whose trigger condition is path-based. File triggers are used
to centralize script patterns that repeat across multiple packages,
such as registering domain-specific files into databases or updating
system-wide caches.

File triggers receive the matching paths via standard input, one per line.

Like triggers, they receive one argument (*$1*), which contains the
number of installed instances of the _triggered package_ (ie. the package
containing the file trigger scriptlet) when the operation has completed.

In addition, *Package File Triggers* receive a second argument (*$2*),
which contains the number of installed instances of the the _triggering package_
(ie. the package which set off the trigger).

The file triggers of a package can be examined with a *rpm -q --filetriggers*
query.

## Package File Triggers
Package file triggers execute once per each triggering package.

*%filetriggerin*
	Executed just after the installation of each triggering package.

*%filetriggerun*
	Executed just before the removal of each triggering package.

*%filetriggerpostun*
	Executed just after the removal of each triggering package.

## Transaction File Triggers
Transaction file triggers only execute once per transaction, regardless
of how many packages triggered it.

*%transfiletriggerin*
	Executed once at the end of a transaction, for all matching installed
	files (ie. from the transaction or previously installed).

*%transfiletriggerun*
	Executed once at the start of a transaction, for all matching removed
	files.

*%transfiletriggerpostun*
	Executed once at the end of a transaction if there were matching
	removed files.

	Note: the list of triggering files is not available in this slot.

# EXECUTION ORDER
The relative execution order of scriptlets within a transaction for a
single package upgrade is as follows. _new_ refers to a newly installed
package version, _old_ refers to the already installed package version
to be replaced by _new_, _rpmdb_ means installed packages, and _any_
means both _rpmdb_ and the other packages in the current transaction.

. *%pretrans* of _new_
. *%preuntrans* of _old_
. *%transfiletriggerun* of _any_, set off by removal of _old_
. *%sysusers* of _new_ (implicit)
. *%triggerprein* of _rpmdb_, set off by install of _new_
. *%triggerprein* of _new_, set off by _rpmdb_
. *%pre* of _new_
. (unpack _new_ files)
. *%filetriggerin* of _any_ (high priority), set off by _new_
. *%filetriggerin* of _new_ (high priority), set off by _any_
. *%post* of _new_
. *%triggerin* of _rpmdb_, set off by _new_
. *%triggerin* of _new_, set off by _rpmdb_
. *%filetriggerin* of _any_ (low priority), set off by _new_
. *%filetriggerin* of _new_ (low priority), set off by _any_
. *%triggerun* of _old_, set off by _rpmdb_
. *%triggerun* of _rpmdb_, set off by _old_
. *%filetriggerun* of _old_ (high priority), set off by _any_
. *%filetriggerun* of _any_ (high priority), set off by _old_
. *%preun* of _old_
. *%filetriggerun* of _old_ (low priority), set off by _any_
. *%filetriggerun* of _any_ (low priority), set off by _old_
. (erase _old_ files)
. *%filetriggerpostun* of _old_ (high priority), set off by _any_
. *%filetriggerpostun* of _any_ (high priority), set off by _old_
. *%postun* of _old_
. *%filetriggerpostun* of _old_ (low priority), set off by _any_
. *%filetriggerpostun* of _any_ (low priority), set off by _old_
. *%posttrans* of _new_
. *%postuntrans* of _old_
. *%transfiletriggerpostun* of _any_, set off by _old_
. *%transfiletriggerin* of _any_, set off by _new_

# ENVIRONMENT
*RPM_INSTALL_PREFIX_*​_N_
	The _N_'th install prefix in relocatable packages.

*RPM_INSTALL_PREFIX*
	Same as *RPM_INSTALL_PREFIX0*, for legacy compatibility.

# EXIT STATUS
A non-zero exit code from any scriptlet will be reflected as an error in the
transaction result code. Additionally, non-zero exit from the various
pre-scriptlets (noted above for each such scriptlet) stop further processing
related to _that package_, not the whole transaction.

Scriptlets should be designed to always return with zero exit code, as RPM
cannot undo or roll back a transaction.

For example, an exit code of 1 from a *%pre* script prevents the install/update
of that package, but the rest of the transaction will still take place,
even if there are missing dependencies.

# EXAMPLES
## Example 1. Update /etc/shell on package install/removal

```
%post
if [ $1 -eq 1 ]; then
    # Initial install
    grep -q '^/bin/mysh$' /etc/shells || echo '/bin/mysh' >> /etc/shells
fi

%postun
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    sed -i '/^\/bin\/mysh$/d' /etc/shells
fi
```

## Example 2. Manage alternatives in a subpackage

```
Name: frobber
[...]

%package myimpl
[...]

%post myimpl
update-alternatives --install /usr/bin/frobber \\
	frobber /usr/bin/frobber.myimpl 10

%preun myimpl
if [ $1 -eq 0]; then
    # Package removal, not upgrade
    update-alternatives --remove frobber /usr/bin/frobber.myimpl 10
fi
```

## Example 3. Manage the MIME database

```
%transfiletriggerin -- /usr/share/mime
/usr/bin/update-mime-database -n /usr/share/mime &> /dev/null ||:

%transfiletriggerpostun -- /usr/share/mime
/usr/bin/update-mime-database -n /usr/share/mime &> /dev/null ||:
```

Note that these would go to the package owning the
_/usr/bin/update-mime-database_ binary, which then centrally handles
all updates to the _/usr/share/mime_ directory. Besides removing the need
to have such scriptlets in all packages with such content, it also eliminates
the need to have dependencies to the helper binary in all related packages.

## Example 4. Enable a systemd preset on upgrade from older version

```
Name: my
Version: 3.0
[...]

# On update from 2.x
%triggerun -- my < 3.0-1
if [ -x /usr/bin/systemctl ]; then
    systemctl --no-reload preset mydb-migrate ||:
fi
```

## Example 5. Execute a program in %pretrans

```
%pretrans -p <lua>
if posix.access('/usr/bin/mything', 'x') then
    rpm.execute('/usr/bin/mything')
end
```

Note that this seemingly simple thing is generally not possible in
*%pretrans* without using *-p <lua>*, as on the initial install of
a system, even _/bin/sh_ is not present at all.

## Example 6. Runtime macro expansion

```
%post -e
ln -s %{_datadir}/bar/some.ext %%{_libdir}/foo/some.ext
```

## Example 7. Runtime queryformat expansion for installed filenames

```
%post -q
for f in [%%{instfilenames} ]; do
    echo $f
done
```

The filenames printed by this accurately reflect any file policies from the
time of installation, such as *--nodocs*.

# BUGS
Priority for *Transaction File Triggers* is not currently implemented.

# SEE ALSO
*rpm*(8), *rpm-lua*(7), *rpm-sysusers*(7), *rpm-spec*(5)
