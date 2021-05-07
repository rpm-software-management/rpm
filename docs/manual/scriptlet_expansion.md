---
layout: default
title: rpm.org - Runtime scriptlet expansion
---
# Runtime scriptlet expansion (DRAFT)

Traditionally rpm scriptlets are macro-expanded at build-time like everything else in specs, but beyond that they are "static". Usually this is just what you want, but there are some cases where one would rather leave some decisions until install-time. For example a noarch package might want to interact with an arch-specific package in its scriptlets, which can involve determining the correct value of %{_libdir}. Or one might want to only perform some actions depending on macros defined on the system. Macros can be evaluated at runtime by using the built-in Lua-interpreter (ie -p <lua> scriptlets) with rpm.expand() but rewriting scriptlets in Lua is not always feasible. Starting with version 4.9.0, rpm supports two generic runtime scriptlet expansion mechanisms that are available to all scriptlets regardless of the language they are written in: macro- and queryformat-expansion.

## Macro expansion

Taking the example above where noarch package wants to create a symlink to a arch-dependent location from its scriptlet, traditionally one might use something like this - figure out whether /usr/lib64 or /usr/lib should be used by looking at existing paths:

```
%post
if [ -d /usr/lib64/foo ]; then
    ln -s %{_datadir}/bar/some.ext /usr/lib64/foo/some.ext
elif [ -d /usr/lib/foo ]; then
    ln -s %{_datadir}/bar/some.ext /usr/lib/foo/some.ext
fi
```

Using runtime macro expansion this could be written as:

```
%post -e
ln -s %{_datadir}/bar/some.ext %%{_libdir}/foo/some.ext
```

The -e argument to %post (or any other scriptlet) enables the runtime macro expansion for this particular script. Also note that %%{_libdir} is escaped with an extra %, otherwise it would be expanded at build-time along with %{_datadir} which is not what you want here. What the scriptlet does now directly depends on the system its being installed on - architecture and overall rpm configuration:

On a single-lib system (such as most 32bit architectures) it would typically expand to:

```
ln -s /usr/share/bar/some.ext /usr/lib/foo/some.ext
```

...and on a multilib-capable 64bit system it would expand to:

```
ln -s /usr/share/bar/some.ext /usr/lib64/foo/some.ext
```

For fancier tricks, all rpm macros including built-ins such as %{lua:...} can be used in this manner. However the effects of %define, %global and %undefine are limited to the particular scriptlet being expanded: the scriptlet expansion occurs in the fork()'ed child process, not in the main rpm process.

## Queryformat expansion

The other form of runtime expansion is queryformat expansion, which can be rather cumbersome to use but allows for something not otherwise possible at all: access to the package header contents at runtime. Much of the data that is static by nature, such as %{name}, %{version} and %{_arch}, is of course available as build-time macros, but there's plenty of data which is simply not available at build-time. Such as the exact file list of the package, not to mention the files that actually are installed when considering effects of --nodocs, %{_netshareddir}, %{_installangs} etc mechanisms. Consider the following simple "hello world" example package with a queryformat-expanded %post scriptlet:

```
%files
%defattr(-,root,root)
%doc FAQ README COPYING
%{_bindir}/hello

%post -q
for f in [%%{instfilenames} ]; do
    echo $f
done
```

The -q argument to %post (or any other scriptlet) enables the runtime queryformat expansion for this particular script. The entire script now needs to be a legal --queryformat argument, which is what makes this form of runtime expansion harder to use: be prepared for lots of annoying escapes for common shell syntaxes such as {} and [].

Here's what a typical installation of the above example would output:

```
[root@localhost ~]# rpm -Uvh /tmp/hello-2.0-1.x86_64.rpm 
Preparing...                          ################################# [100%]
Updating / installing...
   1:hello-2.0-1                      ################################# [100%]
/usr/bin/hello
/usr/share/doc/hello-2.0
/usr/share/doc/hello-2.0/COPYING
/usr/share/doc/hello-2.0/FAQ
/usr/share/doc/hello-2.0/README
[root@localhost ~]#
```

With --nodocs the output from %post looks quite different:

```
[root@localhost ~]# rpm -Uvh --nodocs /tmp/hello-2.0-1.x86_64.rpm 
Preparing...                          ################################# [100%]
Updating / installing...
   1:hello-2.0-1                      ################################# [100%]
/usr/bin/hello
[root@localhost ~]# 
```

## Other notes

It's possible to combine both forms of runtime scriptlet expansion, in which case macros are expanded first and the result is used as the queryformat string. However given all the conflicts between macro and queryformat syntaxes, on top of all all the shell-escaping involved, this is perhaps best avoided.

As mentioned earlier, only rpm >= 4.9.0 supports runtime scriptlet expansion, older versions can not correctly be used to install packages utilizing it. The following rpmlib() dependency is automatically added to guard against installation with rpm versions which do not support it:

```
rpmlib(ScriptletExpansion) <= 4.9.0-1
```



