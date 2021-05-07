---
layout: default
title: rpm.org - Dependencies
---
# Dependencies

Dependencies provide a way for a package builder to require other packages or capabilities to be installed before or simultaneously with one another. For example, these can be used to require a python interpreter for a python-based application. RPM ensures dependencies are satisfied whenever packages are installed, erased, or upgraded. This page describes the basics. More topics can be found in More On Dependencies.

## The Package name
The package name is the most important way to refer to a package. Updates also follow the package name and packages with the same name constitute a line of updates of which only the newest is considered up-to-date. So if two versions of the same software are supposed to be installed at the same time, they have to go into packages with different names, which of course need to reside at different locations on disk). A typical way of doing this is to add the unchanging part of the version number as part of the name, e.g. `python3-3.2.1-1` and `python-2.3.4-5` could be installed at the same time.

## Obsoletes
Obsoletes alter the way updates work. This plays out a bit differently depending whether rpm is used directly on the command line or the update is performed by an updates/dependency solver.

RPM removes all packages matching obsoletes of packages being installed, as it sees the obsoleting package as their updates. There does not have to be a one-to-one relationship between obsoleting and obsoleted packages. Note that `rpm -i` does not do updates and therefore treats Obsoletes: as Conflicts: (since rpm-4.10). For most cases `rpm -i` should be avoided and `rpm -U` should be used unless this specific behavior is desired (e.g. for kernels).

Updaters have a slightly different view on Obsoletes: as they need to find out what packages to install as an update. As a result packages containing matching Obsoletes: are added as updates and replace the matching packages.

## Provides
Provides can be added to packages so they can be referred to by dependencies other than by their name. This is useful when you need to make sure that the system your package is being installed on has a package which provides a certain capability, even though you don't care what specific package provides it. For example, sendmail won't work properly unless a local delivery agent (lda) is present. You can ensure that one is installed like this:

```
    Requires: lda
```

This will match either a package called lda (as mentioned above), or any package which contains:

```
    Provides: lda
```

in its spec file. No version numbers may be used with virtual packages.

Provides are often used to supply file dependencies such as /bin/sh on machines that are only partly managed by rpm. A virtual package with

```
    Provides: /bin/sh
```

differs from a package that has `/bin/sh` in the %files list in that the package can be safely removed without removing `/bin/sh`.

## Requires
With this tag a package can require another with the matching name or Provides to be installed (if the package containing the Requires: is going to be installed). This is checked when a new package is installed and if a package with a matching Provides: is removed.

To require the packages python and perl, use:

```
    Requires: python perl
```

in the spec file. Note that "Requires: python, perl" would work as well. If you needed to have a very recent version of python but any version of perl,

```
    Requires: python >= 1.3, perl
```

would do the trick. Again, the ',' in the line is optional. Instead of '>=', you may also use '<', '>', '<=', or '='. Spaces are required around the numeric operator to separate the operator from the package name.

## Versioning
The full syntax for specifying a dependency on an epoch, version and release is

```
    [epoch:]version[-release]
```

where

```
    epoch   (optional) number, with assumed default of 0 if not supplied
    version (required) can contain any character except '-'
    release (optional) can contain any character except '-'
```

For example,

```
    Requires: perl >= 9:5.00502-3
```

specifies

```
    epoch=9
    version=5.00502
    release=3
```

The epoch (if present) is a monotonically increasing integer, neither the version or the release can contain the '-' hyphen character, and the dependency parser does not permit white space within a definition. Unspecified epoch and releases are assumed to be zero, and are interpreted as "providing all" or "requiring any" value.

The release tag is usually incremented every time a package is rebuilt for any reason, even if the source code does not change. For example, changes to the specfile, compiler(s) used to build the package, and/or dependency changes should all be tracked by incrementing the release. The version number, on the other hand, is usually set by the developer or upstream maintainer, and should not be casually modified by the packager.

Version numbering should be kept simple so that it is easy to determine the version ordering for any set of packages. If the packager needs to separate a release from all other releases that came before it, then the epoch, the most significant part of package ordering, can be changed.

The algorithm that RPM uses to determine the version ordering of packages is simple and developers are encouraged not to rely on the details of its working. Developers should keep their numbering scheme simple so any reasonable ordering algorithm would work. The version comparison algorithm is in the routine rpmvercmp() and it is just a segmented strcmp(3). First, the boundaries of the segments are found using isdigit(3)/isalpha(3). Each segment is then compared in order with the right most segment being the least significant. The alphabetical portions are compared using a lexicographical ascii ordering, the digit segments strip leading zeroes and compare the strlen before doing a strcmp. If both numerical strings are equal, the longer string is larger. Notice that the algorithm has no knowledge of decimal fractions, and perl-5.6 is "older" than perl-5.00503 because the number 6 is less than the number 503.

The concept of "newer" used by rpm to determine when a package should be upgraded can be broken if version format changes oddly, such as when the version segments cannot be meaningfully compared.

Example of a bad format change: 2.1.7Ax to 19980531

```
  The date may be the older version, but it is numerically greater
  2 so it is considered newer :(
```

Example of a bad increment: 2.1.7a to 2.1.7A

```
  The 'a' (ASCII 97) is compared against 'A' (ASCII 65), making 2.1.7a
  the newer version.
```

Stick to major.minor.patchlevel using numbers for each if you can. Keeps life simple :-)

If a Requires: line needs to include an epoch in the comparison, then the line should be written like

```
    Requires: somepackage = 23:version
```

You can't continue a "Requires: " line. If you have multiple "Requires: " lines then the package requires all packages mentioned on all of the lines to be installed.

## Conflicts
Conflicts are basically inverse Requires. If there is a matching package the package cannot be installed. It does not matter whether the Conflict: tag is on the already installed or to be installed package.

The qmail spec file may codify this with a line like:

```
    Conflicts: sendmail
```

Dependency checking (including checking for conflicts) may be overridden by using the --nodeps flag.

## Weak dependencies

In addition to the strong dependencies created by Requires, there are 4 dependencies that are completely ignored by rpm itself. Their purpose is to be used by dependency solvers to make choices about what packages to install. They come in two levels of strength:

* Weak: By default the dependency solver shall attempt to process the dependency 
as though it were strong. If this is results in an error then they should be ignored and not trigger an error or warning.

* Very weak: By default the dependency solver shall ignore them. But they may be used to show the matching packages as option to the user. 

The depsolver may offer to treat the weak like very weak relations or the other way round.

In addition to normal, forward relations that behave the same way as Requires:, there are also two weak dependencies that work backwards. Instead of adding packages that match the relations of to-be-installed packages these Relations add packages that contain relations matching to-be-installed packages.

There are two dependency types at each level of strength. One is a forward relation, similar to Requires; the other is a reverse relation. For reverse relation the roles of the declaring and matching package(s) are switched out.

|           |Forward      |Reverse       |
|-----------|-------------|--------------|
|Weak       |Recommends:  |Supplements:  |
|Very weak  |Suggests:    |Enhances:     |

So installing a package containing Recommends: foo should cause the dependency solver to also select a package that is named foo or that Provides: foo, assuming one exists and its selection does not lead to unresolvable dependencies.

On the other hand, if a package that is named foo or that Provides: foo is selected, and a package bar containing "Supplements: foo" exists, then bar is also selected as long as doing so does not lead to unresolvable dependencies.

In other words, if you have packages A and B and you want to declare a weak relation between them A -> B, you can either specify it as package A containing "Recommends: B" or package B containing "Supplements: A".

