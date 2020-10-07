# Relocatable packages

Relocatable packages are a way to give the user a little control
over the installation location of a package.  For example, a vendor
may distribute their software to install in "/opt" but you'd like
it to install in "/usr/opt".  If the vendor were distributing a
relocatable RPM package, it would be easy.

## Building a Relocatable Package

Not all software can be "relocatable".  Before continuing you should
think about how the program works, what files it accesses, what other
programs access *it* (and expect it to be in a certain place), etc.
If you determine that the location of the package doesn't matter,
then it can probably be built as "relocatable".

All you need to do to build a relocatable package is put one or more:

```
  Prefix: <dir>
```

in your spec file.  The "<dir>" will usually be something like "/usr",
"/usr/local", or "/opt".  Every file in your %files list must start
with that prefix.  For example, if you have "Prefix: /usr" and your
%files list contains "/etc/foo.conf", the build will fail. The fix for
this is to put

```
  Prefix: /usr
  Prefix: /etc
```

into the spec file so that the /usr and /etc directories may be
relocated separately when this package is installed.


## Installing Relocatable Packages

By default, RPM will install a relocatable package in the prefix
directory listed in the spec file.  You can override this on the
RPM install command line with "--prefix <dir>".  For example, if
the package in question were going to be installed in "/opt" but
you don't have enough disk space there (and it is a relocatable
package), you could install it "--prefix /usr/opt".

If there is more then one Prefix you may relocate each prefix
separately by using syntax like:

```
  rpm ... --relocate /opt=/usr/opt --relocate /etc=/usr/etc ...
```

If any of the Prefixes is not being relocated they can be skipped on
the command line
