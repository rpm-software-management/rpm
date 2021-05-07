---
layout: default
title: rpm.org - More on Dependencies
---
# More on Dependencies

## Boolean Dependencies

Starting with rpm 4.13.0, rpm supports boolean expressions in all
dependencies. For example:

```
	Requires: (pkgA or (pkgB and pkgC))
```

More details in a [separate article](boolean_dependencies.html).

## Architecture-specific Dependencies

Starting with rpm 4.6.0, it's possible to specify dependencies to be specific to a given architecture family:

```
         Requires: somepackage%{?_isa} = version
```

More details in a [separate article](arch_dependencies.html).

## Scriptlet Dependencies
Often package scriptlets need various other packages in order to execute correctly, and sometimes those dependencies aren't even needed at runtime, only for installation. Such install/erase-time dependencies can be expressed with "Requires(<scriptlet>): <dependency>" notation, for example the following tells rpm that useradd program is needed by the package %pre scriptlet (often the case if a package uses a custom group/username for its files):

```
        Requires(pre): /usr/sbin/useradd
```

This has two "side-effects":
* It ensures that the package providing /usr/sbin/useradd is installed before this package. In presence of dependency loops, scriptlet dependencies are the only way to ensure correct install order.
* If there are no other dependencies on the package providing /usr/sbin/useradd, that package is permitted to be removed from the system after installation(!) 

It's a fairly common mistake to replace legacy PreReq dependencies with Requires(pre), but this is not the same, due to the latter point above!

## Automatic Dependencies
To reduce the amount of work required by the package builder, RPM scans the file list of a package when it is being built. Any files in the file list which require shared libraries to work (as determined by ldd) cause that package to require the shared library.

For example, if your package contains /bin/vi, RPM will add dependencies for both libtermcap.so.2 and libc.so.5. These are treated as virtual packages, so no version numbers are used.

A similar process allows RPM to add Provides information automatically. Any shared library in the file list is examined for its soname (the part of the name which must match for two shared libraries to be considered equivalent) and that soname is automatically provided by the package. For example, the libc-5.3.12 package has provides information added for libm.so.5 and libc.so.5. We expect this automatic dependency generation to eliminate the need for most packages to use explicit Requires: lines.

## Custom Automatic Dependency
Customizing automatic dependency generation is covered in [dependency generator documentation]().

## Interpreters and Shells
Modules for interpreted languages like perl and tcl impose additional dependency requirements on packages. A script written for an interpreter often requires language specific modules to be installed in order to execute correctly. In order to automatically detect language specific modules, each interpreter may have its own [generators](dependency_generators.html). To prevent module name collisions between interpreters, module names are enclosed within parentheses and a conventional interpreter specific identifier is prepended:

```
  Provides: perl(MIME-Base64), perl(Mail-Header)-1-09

  Requires: perl(Carp), perl(IO-Wrap) = 4.5
```

The output of a per-interpreter dependency generator (notice in this example the first requirement is a package and the rest are language specific modules)

```
    Mail-Header >= 1.01
    perl(Carp) >= 3.2
    perl(IO-Wrap) == 4.5 or perl(IO-Wrap)-4.5
```

the output from dependency generator is

```
    Foo-0.9
    perl(Widget)-0-1
```

## Installing and Erasing Packages with Dependencies
For the most part, dependencies should be transparent to the user. However, a few things will change.

First, when packages are added or upgraded, all of their dependencies must be satisfied. If they are not, an error message like this appears:

```
    failed dependencies:
        libICE.so.6  is needed by somepackage-2.11-1
        libSM.so.6  is needed by somepackage-2.11-1
        libc.so.5  is needed by somepackage-2.11-1
```

Similarly, when packages are removed, a check is made to ensure that no installed packages will have their dependency conditions break due to the packages being removed. If you wish to turn off dependency checking for a particular command, use the --nodeps flag.

## Querying for Dependencies
Two new query information selection options are now available. The first, --provides, prints a list of all of the capabilities a package provides. The second, --requires, shows the other packages that a package requires to be installed, along with any version number checking.

There are also two new ways to search for packages. Running a query with --whatrequires \<item\> queries all of the packages that require \<item\>. Similarly, running --whatprovides \<item\> queries all of the packages that provide the \<item\> virtual package. Note that querying for package that provides "python" will not return anything, as python is a package, not a virtual package.

## Verifying Dependencies
As of RPM 2.2.2, -V (aka --verify) verifies package dependencies by default. You can tell rpm to ignore dependencies during system verification with the --nodeps. If you want RPM to verify just dependencies and not file attributes (including file existence), use the --nofiles flag. Note that "rpm -Va --nofiles --nodeps" will not verify anything at all, nor generate an error message.

## Branching Version
It is quite common to need to branch a set of sources in version control. It is not so obvious how those branches should be represented in the package version numbers. Here is one solution.

You have a bag of features that are injected into a package in a non-ordered fashion, and you want to have the package name-version-release be able to:

```
    1) identify the "root version" of the source code.
    2) identify the handful of features that are in that
       branch of the package.
    3) preserve sufficient ordering so that packages upgrade
       without the use of --oldpackage.
```

A simple (but possibly not adequate) scheme to achieve this is:

```
    Name: foo
    Version: <the "root version" of the source code>
    Release: <release instance>.<branch>
```

where the release instance is something like YYYMMMDD or some linear record of the number of builds with the current tar file, it is used to preserve ordering when necessary.

Another alternative scheme might be:

```
    Name: foo
    Epoch: <branch>
    Version: <the branch specific version of the code>
    Release: <release instance>
```

## Build dependencies
The following dependencies need to be fullfilled at build time. These are similar to the install time version but these apply only during package creation and are specified in the specfile. They end up as "regular" dependencies of the source package (SRPM) (BuildRequires? become Requires) but are not added to the binary package at all.

```
    BuildRequires:
    BuildConflicts:
    BuildPreReq:
```
