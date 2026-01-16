RPM-DEPENDENCIES(7)

# NAME
*rpm-dependencies* - RPM dependencies

# SYNOPSIS
## Capability
_NAME_ [_RANGE_]

## Dependency
_NAME_[*(*_MODIFIER_[*,*_MODIFIER_ ...]*)*] [_RANGE_]

## Boolean dependency
*(*_DEPENDENCY_ _OPER_ _DEPENDENCY_ ...*)*

# DESCRIPTION
RPM uses _dependencies_ to describe various relations among packages,
such as what _capabilities_ are needed for software to function,
and other constraints such as conflicting with another package.
Dependencies are _resolved_ before package operations such as install,
update and erase to ensure the constraints of all packages are fulfilled.
RPM also uses the dependency information for calculating the
installation and erase _ordering_.

Capabilities and dependencies on them consist of a capability name,
optionally followed by version range information. 

Capability _NAME_ must start with an alpha-numeric, underscore (*\_*)
or slash (*/*). _RANGE_ consists of a comparison operator
(*<*, *<=*, *=*, *>=*, *>*, *<>*) and an *rpm-version*(7) label called _EVR_.

Dependencies can be either _file-backed_, such as the path of a packaged
file or a dependency generated from such a file, or _virtual_. Most
automatically generated dependencies are file-backed. All dependencies
added manually to a spec are virtual.

## Capabilities
*Provides*
	Capabilities provided by a package (or the system).

	Note: Although commonly referred to as such, provides themselves are
	not dependencies. They merely describe the capabilities that can be
	depended on.

## Dependencies
*Requires*
	Capabilities a package requires to function.

*Conflicts*
	Capabilities a package conflicts with. Typically packages
	with conflicting paths or otherwise conflicting functionality,
	such as using the same network port.

*Obsoletes*
	Packages obsoleted by a package. Used for replacing and
	renaming packages.

	Note: in contrast to all other dependency types, *Obsoletes*
	are matched against package *Name* and EVR rather than *Provides*
	capabilities.

## Weak dependencies
Weak dependencies are group of optional *Requires*-like dependencies.
RPM only uses the weak dependency information for ordering hints,
they are mainly targeted at dependency solver programs.

*Recommends*
	Optional capabilities recommended by a package for the best user
	experience. 

*Suggests*
	Optional capabilities that provide additional functionality for
	a package.

*Supplements*
	Reverse *Recommends*: this package is recommended for another
	package for the best user experience.
	Mainly useful for 3rd party enhancements.

*Enhances*
	Reverse *Suggests*: this package provides additional functionality
	for another package. Mainly useful for 3rd party enhancements.

*OrderWithRequires*
	Optional *Requires* for ordering calculations. IFF the capability
	is present in a transaction, calculate ordering as if it was a
	*Requires*.

## Build dependencies
Build dependencies only exist in the context of building a package
with *rpmbuild*(1). They are not carried into binary packages at all,
and are recorded as regular *Requires* and *Conflicts* in source rpms.

*BuildRequires*
	Capabilities that musth be present during the build of the package.

*BuildConflicts*
	Capabilities that must not be present during the build of the package.

## Modifiers
Most dependency types accept additional modifiers to change how and when
that dependency is enforced. Some modifiers make a dependency _transient_:
it only applies to certain times of installation or erasure. Others make
a dependency _unordered_, causing it to be dismissed from ordering
calculations. _Ordered_ modifiers cause the dependency to be favored
when breaking loops for ordering purposes.

*config*
	Denotes *%config* file presence in the package.
	Added automatically by RPM, mostly considered a historic remnant.

*interp*
	Denotes a scriptlet interpreter dependency, usually added automatically
	by rpm. Used as a strong ordering hint for breaking dependency loops.

*meta* (since rpm >= 4.16)
	Denotes a "meta" dependency, which must not affect transaction
	ordering. Typical use-cases would be meta-packages and sub-package
	cross-dependencies whose purpose is just to ensure the sub-packages
	stay on common version.

*post*
	Denotes the dependency must be present right after the package is
	is installed, and is used a strong ordering hint to break possible
	dependency loops. A *post*-dependency is free to be removed
	once the install-transaction completes.

	Also relates to *%post* scriptlet execution.

*posttrans*
	Denotes the dependency must be present at the end of transaction, ie
	cannot be removed during the transaction.

	Also relates to *%posttrans* and *%postuntrans* scriptlet execution.

*postun*
	Denotes the dependency must be present right after the package is
	is removed, and is used a strong ordering hint to break possible
	dependency loops.

	Also relates to *%postun* scriptlet execution.

*pre*
	Denotes the dependency must be present in before the package is
	is installed, and is used a strong ordering hint to break possible
	dependency loops. A pre-dependency is free to be removed
	once the install-transaction completes.

	Also relates to *%pre* scriptlet execution.

*pretrans*
	Denotes the dependency must be present at the start of a transaction.
	Such a dependency cannot be satisfied by installing other packages
	in the same transaction.

	Also relates to *%pretrans* and *%preuntrans* scriptlet execution.

*preun*
	Denotes the dependency must be present in before the package is
	is removed, and is used a strong ordering hint to break possible
	dependency loops.

	Also relates to *%preun* scriptlet execution.

*verify*
	Relates to *%verify* scriptlet execution. As the *%verify* scriptlet
	is not executed during install/erase, this does not affect transaction
	ordering.


Multiple modifiers can be supplied separated by comma, as long as
they're not semantically contradictory: *meta* modifier contradicts any
ordered modifier, eg *meta* and *verify* can be combined, and *pre* and
*verify* can be combined, but *pre* and *meta* can not.

As noted above, install-time only dependencies (*pretrans*,
*pre*, *post*, *posttrans* or combination of them) can be removed after the
installation transaction completes if there are no other dependencies
to prevent that. This is a common source of confusion.

Modifiers in packages can be examined by adding *-v* to *rpm*(8) dependency
queries, eg. *rpm -qv --requires*. Dependencies added by the
*rpm-dependency-generators*(7) are marked *auto* in the output.

## Boolean dependencies
RPM can process boolean expressions in
Requires, Recommends, Suggests, Supplements, Enhances and Conflicts type
dependencies. This allows for complex dynamic behavior whereas traditional
dependencies are static in nature.

Boolean expressions are always enclosed with parenthesis.
Boolean dependencies are built out of two or more dependencies (operands)
and boolean operators:

*(*_DEPENDENCY_ _OPER_ _DEPENDENCY_ ...*)*

Operands can be boolean expressions themselves. They also need to be surrounded
by parenthesis. *and* and *or* operators may be chained together repeating the
same operator with only one set of surrounding parenthesis.

The semantics of the dependencies are unchanged, but instead of checking for
one match, all names are checked and the boolean value of a match is then
aggregated over the boolean operators. For a conflict to not prevent an install
the result has to be _false_, for everything else it has to be _true_.

## Boolean operators
*and*
	Requires all operands to be fulfilled for the term to be True.

	Example:
	```
	Conflicts: (pkgA and pkgB)
	```
*or*
	Requires one of the operands to be fulfilled.

	Example:
	```
	Requires: (pkgA >= 3.2 or pkgB)
	```
*if*
	Requires the first operand to be fulfilled if the second is
	(reverse implication).

	Example:
	```
	Recommends: (myPkg-langCZ if langsupportCZ)
	```
*if else*
	Same as *if*, but require the third operand to be fulfilled if the
	second is not.

	Example:
	```
	Requires: (myPkg-backend-mariaDB if mariaDB else sqlite)
	```
*with*
	Requires all operands to be fulfilled by the same package for the
	term to be True.

	Example:
	```
	Requires: (pkgA-foo with pkgA-bar)
	```
*without*
	Requires a single package that satisfies the first operand but not
	the second (set subtraction).

	Example:
	```
	Requires: (pkgA-foo without pkgA-bar)
	```
*unless*
	Requires the first operand to be fulfilled if the second is not
	(reverse negative implication).

	Example:
	```
	Conflicts: (myPkg-driverA unless driverB)
	```
*unless else*
	Same as *unless* but requires the third operand to be fulfilled if
	the second is.

	Example:
	```
	Conflicts: (myPkg-backend-SDL1 unless myPkg-backend-SDL2 else SDL2)
	```

Note: the *if* operators cannot be used in the same context with *or*, and
the *unless* operators cannot be used in the same context with *and*.

Added: 4.14

# RESOLVING
Dependencies are resolved by matching them one at a time against available
capabilities, taking into account implications of package operations like
install, update and erase.

A dependency match is performed by looking up the providers of the capability
name. For versionless dependencies, any match is sufficient. For versioned
dependencies, the ranges must overlap to be satisfied.

Note that there can be arbitrary number of provides for a given capability,
and different dependencies can be resolved to different providers. This
can lead to situations where seemingly nonsensical constraints such as
*Requires: abc < 1.0* and *Requires: abc > 2.0* are simultaneously met.
The boolean operators *with* and *without* can be used to pinpoint
capabilities to certain packages.

For most dependency types, satisfied is the desired state of things, but
- A satisfied conflict indicates a problem
- A satisfied obsolete indicates a package replaces another

## Range overlap
Both capabilities and dependencies can include a version range, which
can either be a single _EVR_, or a range.

For most things, the match rules are obvious: capability *= 1.0-1* matches
a *< 2.0* dependency, *= 2.0* does not match *= 1.0*, and *< 2.0* does
not match *> 2.0*. There are some notable gotchas however:

- A capability _EVR_ with no release component matches _any release_ of
  that version. For example, capability *= 1.2.3* matches *>= 1.2.3-5*
- A versionless capability matches _any_EVR_. For example, *Provides: abc*
  matches *Requires: abc >= 1:5.0-2*.
- Note the significance of *Epoch* on both sides: a *= 1:1.0* capability does
  not match *<= 1.0* and a *= 1.0* capability does not match *>= 1:1.0*

## Package provides
Package provides include all the capabilities that were added either
by *rpm-dependency-generators*(7) during the package build, or manually
in the *rpm-spec*(5).

In addition, all packages explicitly provide their own _NEVR_:

_NAME_ = [_EPOCH_:]_VERSION_-_RELEASE_

Similarly, all architecture dependent packages explicitly provide an ISA
dependency which can be used for requiring an specific architecture family
among multiple providers:

_NAME_*(*_ISA_*)* = [_EPOCH_:]_VERSION_-_RELEASE_

The *%{\_isa}* macro can be used to populate the _ISA_ value in an
*rpm-spec*(5). ISA consists of two parts separated with a dash (*-*):
the name of the architecture family in *%\_\_isa\_name* and its
bitness in *%\_\_isa\_bits*.

## Implicit file provides
All packages implicitly provide the pathnames of the contained files.
Packages can thus depend on an arbitrary path, such as _/bin/sh_ to
request the standard shell interpreter without specifying a package name
explicitly.

These differ from manually added provides of equal values in two ways.
Implicit file provides are
- not visible in a packages provides dependencies (eg. *--provides* query)
- subject to package install policies such as *--nodocs*:
  files that are not (being) installed are not provided

## System provides
Most dependencies are provided by other packages, but there are some
exceptions where the running system is used for provide matching.
As a general rule, capabilities provided by the system are free of
ordering constraints.

System users and groups usually originate from other installed packages,
but they can also originate from a central name service system.
To allow packaged content to reference such users and groups,
*user()* and *group()* requires are first matched against the system
*nss*(5), and only then other packages.

## rpmlib() dependencies
RPM uses special *rpmlib()* dependencies to track feature compatibility.
Packages utilizing features that are not supported in all RPM versions
are automatically flagged with an *rpmlib(*_CAPABILITY_*)*
requires-dependency by *rpmbuild*(1). Some of these are
compile-time options when building RPM, others are RPM version-dependent.

*rpmlib()* requires cannot be satisfied by other packages.
Instead, they dependencies are matched against the RPM process performing
the installation. The *rpmlib()* features supported by the system
RPM can be inspected by running *rpm --showrc*.

# ORDERING
RPM uses the dependency information in packages to calculate the install
and erase ordering. The basic principle is simple: any Requires of a package
must be present at the time of it's install and erase. In other words, if
package A requires B and C, A must be installed last of the three, and
erased first.

A dependency loop has no correct ordering, so they should be avoided when
packaging. Common techniques are splitting packages to smaller sub-packages,
using unordered modifiers (*meta* in particular) where appropriate,
using *rpm-sysusers*(7) for managing user and group creation and rewriting
scriptlets to use *rpm-lua*(7) instead of external commands.

An otherwise equal A <-> B loop can be broken one way or the other by
an ordered modifier (eg. *pre*, *post*, *preun* or *postun*), but these
tend to add up and then lose their efficiency.

Weak dependencies are only considered for ordering if they have ordered
modifiers, but even then the modifiers are stripped to avoid overuling
modifiers from hard requires.

Note that RPM always performs all installs first, and erasures last.
This means conflicting packages may temporarily coexist within a transaction,
as long as the end result is no conflicts.

To analyze ordering issues, run *rpm*(8) with the *--deploops* option.

# EXAMPLES
## Example 1. A versionless dependency
Require any version of a package named python. 
```
Requires: python
```

## Example 2. Versioned dependencies

Require at least version 3.2 of python. It's the most common form of
dependencies, used to track documented upstream compatibility range
and similar.
```
Requires: python >= 3.2
```

Require at least version 3.2, release 7 of python. Commonly used in
distributions where a certain release of a package includes a downstream
change that this package needs to function.
```
Requires: python >= 3.2-7
```

Require an exact version of a capability. Possible uses include
ABI compatibility tracking.
```
Requires: python = 3.1.0
```

Require the exact version-release of a capability. Commonly used in
dependencies between sub-packages coming from a single spec, rarely
useful elsewhere:
```
Requires: python = 3.1.0-5
```

## Example 3. File dependency
Require the traditional shell at _/bin/sh_, regardless of which package
contains it:
```
Requires: /bin/sh
```

## Example 4. ISA dependencies
Declare a dependency on the same architecture family of the package:

```
Requires: libbar-devel%{?\_isa} >= 2.2
```

When built for the *x86_64* architecture, this becomes
```
Requires: libbar-devel(x86-64) >= 2.2
```

When built for the *x86* family (i386, i586, i686 etc.) it becomes
```
Requires: libbar-devel(x86-32) >= 2.2
```

Require a 32bit version of a package along with the 64bit version:
```
Requires: foo%{\_isa}
%if %{\_\_isa\_bits} == 64
Requires: foo(%{\_\_isa\_name}-32)
%endif
```

## Example #5. Boolean dependencies
Require one of *pkgA*, *pkgB* or *pkgC*:
```
Requires: (pkgA or pkgB or pkgC)
```

Require either at least version 3.2 of *pkgA*, or any version of *pkgB*:
```
Requires: (pkgA >= 3.2 or pkgB)
```

Require *pkgA*, or both *pkgB* and *pkgC*:
```
Requires: (pkgA or (pkgB and pkgC))
```

Require *rpm-plugin-audit* if *audit* is installed:
```
Require: (rpm-plugin-audit if audit)
```

Require either *pkgA* that also provides capability *capB*, or
*pkgB* that does not have capability *capA*
```
Requires: ((pkgA with capB) or (pkgB without capA))
```

Recommend installation along *driverA* and *driverA-tools* unless *driverB*
is installed:
```
Supplements: ((driverA and driverA-tools) unless driverB)
```

Recommend installation of *myPkg-langCZ* and one of *font1-langCZ*
or *font2-langCZ* if/when *la* is installed.
```
Recommends: ((myPkg-langCZ and (font1-langCZ or font2-langCZ)) if la
```

# SEE ALSO
*rpm*(8) *rpmbuild*(1) *rpm-common*(8) *rpm-spec*(5)
*rpm-dependency-generators*(7) *rpm-sysusers*(7) *rpm-version*(7)
*rpm-scriptlets*(7)

*http://www.rpm.org/*
