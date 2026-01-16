RPM-DEPENDENCIES(7)

# NAME
*rpm-dependencies* - RPM dependency system

# SYNOPSIS
## Capability
_NAME_ [_RANGE_]

## Dependency
_RELATION_*:* _CAPABILITY_ ...

## Core dependencies
*Provides:* _CAPABILITY_ ...++
*Requires*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...++
*Conflicts:* _CAPABILITY_ ...++
*Obsoletes:* _CAPABILITY_ ...

## Weak dependencies
*Recommends*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...++
*Suggests*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...++
*Supplements*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...++
*Enhances*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...++
*OrderWithRequires*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...

## Build dependencies
*BuildRequires:* _CAPABILITY_++
*BuildConflicts:* _CAPABILITY_

## Boolean dependencies
*(*_DEPENDENCY_ _OPERATOR_ _DEPENDENCY_ ...*)*

# DESCRIPTION
RPM uses a mechanism called _dependencies_ to track and enforce various
constraints and other relations that packages have.

Dependencies are expressed in terms of _capability_ relations.
There are many different relation types. The most foundational ones are the
_provider_ of a capability and the functional _requirements_ of the software.
There are also various recommendation type relations, and finally,
conflicts and replaces relations.

Dependencies are _resolved_ before package operations such as install,
update and erase, to ensure the constraints of all packages are fulfilled.
RPM also uses the dependency information for calculating the
installation and erase *ORDERING*.

Dependencies can either be generated automatically from packaged files using
*rpm-dependency-generators*(7), or declared manually in an *rpm-spec*(5) file,
using the syntax described in this manual.

## Capabilities
A capability consists of a name, optionally followed by version
range information:

	_NAME_ [_OPERATOR_ _EVR_]

A capability _NAME_ must start with an alpha-numeric, underscore (*\_*)
or slash (*/*). The optional range consists of a comparison _OPERATOR_
(*<*, *<=*, *=*, *>=*, *>*, *<>*) and an *rpm-version*(7) label called _EVR_.

## Core dependencies
Core dependencies are absolute and must be satisfied for a successful
transaction.

*Provides:* _CAPABILITY_ ...
	Capabilities provided by a package (or the system).

	*Provides* relations form the basis of the dependency system, they
	declare the capabilities that all the other dependencies relate to.

	Abstract provide capabilities (commonly called _virtual provides_)
	can be used to support alternative implementations. For example all
	web browsers in a distribution providing a *webclient* capability
	avoids the need to hardcode browser names into other packages.

*Requires*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...
	Capabilities a package requires to function.

*Conflicts:* _CAPABILITY_ ...
	Capabilities a package conflicts with. Typically packages
	with conflicting paths or otherwise conflicting functionality,
	such as using the same network port.

*Obsoletes:* _CAPABILITY_ ...
	Packages obsoleted by a package. Used for replacing and
	renaming packages.

	Note: in contrast to all other dependency types, *Obsoletes*
	are matched against package *Name* and EVR rather than *Provides*
	capabilities.

## Weak dependencies
Weak dependencies are only hints that are not enforced by RPM, they
are mainly targeted at higher level package managers (see *rpm-design*(7)).
RPM only uses weak dependencies for ordering purposes.

*Recommends*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...
	Optional capabilities recommended by a package for the best user
	experience.

*Suggests*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...
	Optional capabilities that provide additional functionality for
	a package.

*Supplements*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...
	Reverse *Recommends*: this package is recommended for another
	package for the best user experience.
	Mainly useful for 3rd party enhancements.

*Enhances*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...
	Reverse *Suggests*: this package provides additional functionality
	for another package. Mainly useful for 3rd party enhancements.

*OrderWithRequires*[*(*_QUALIFIER_[*,*_QUALIFIER_ ...]*)*]*:* _CAPABILITY_ ...
	Optional *Requires* for ordering calculations. IFF the capability
	is present in a transaction, calculate ordering as if it was a
	*Requires*.

## Build dependencies
Build dependencies only exist in the context of building a package
with *rpmbuild*(1). They are not carried into binary packages at all,
and are recorded as regular *Requires* and *Conflicts* in source rpms.

*BuildRequires:* _CAPABILITY_ ...
	Capabilities that must be present during the build of the package.

*BuildConflicts:* _CAPABILITY_ ...
	Capabilities that must not be present during the build of the package.

## Qualifiers
Most dependency types accept additional qualifiers to change how and when
that dependency is enforced.

Many qualifiers have *rpm-scriptlets*(7) counterparts, and are commonly
considered dependencies for that scriptlet. However the qualifiers refer
to the transaction slot at which a given scriptlet runs, not the scriptlet
itself. Thus, a package can have for example a *Requires(pretrans)*
dependency despite not containing a *%pretrans* scriptlet.

Properties:
- *o*: Ordered, used to determine loop breaking points in transaction
  *ORDERING*.
- *s*: Relates to an *rpm-scriptlets*(7) slot.
- *t*: Transient dependency that only applies during the slot.
- *u*: Unordered, does not participate in transaction *ORDERING*.

*config*
	Denotes *%config* file presence in the package.
	Added automatically by RPM, mostly considered a historic remnant.

	Properties: u

*interp*
	A scriptlet interpreter dependency, added automatically by RPM.

	Properties: N/A

*meta*
	A "meta" dependency, which must not affect transaction
	ordering. Typical use cases would be meta-packages and sub-package
	cross-dependencies whose purpose is just to ensure the sub-packages
	stay on common version.

	Properties: u

	Added: 4.16.0

*post*
	The dependency must be present at the *%post* scriptlet slot.

	Properties: ost

*posttrans*
	The dependency must be present at the *%posttrans* and *%postuntrans*
	scriptlet slot. Such a dependency can only be satisfied by a provider
	that will be present on the system after the transaction finishes.

	Properties: sut

*postun*
	The dependency must be present at the *%postun* scriptlet slot.

	Properties: os

*pre*
	The dependency must be present at the *%pre* scriptlet slot.

	Properties: ost

*pretrans*
	The dependency must be present at the the *%pretrans* and *%preuntrans*
	scriptlet slot. Such a dependency can only be satisfied by
	pre-existing providers, packages in the same transaction are not
	considered at all.

	Properties: sut

*preun*
	The dependency must be present during the *%preun* scriptlet
	execution.

	Properties: os

*verify*
	The dependency must be present during *%verify* scriptlet execution.
	As the *%verify* scriptlet is not executed in transactions,
	this does not affect transaction ordering.

	Properties: u

Multiple qualifiers can be supplied separated by comma, as long as
they're not semantically contradictory: *meta* contradicts any
ordered qualifier, eg. *meta* and *verify* can be combined, and *pre* and
*verify* can be combined, but *pre* and *meta* can not.

As noted above, transient dependencies only apply during their slot, rather
than the package lifetime. For example a *Requires(pre)* dependency no longer
applies once the package has been installed, and the dependency target
can be freely removed. This is a common source of confusion. To turn
such a dependency into a lifetime dependency, *Requires(pre,postun)*
can be used while avoiding duplicating the dependency.

Qualifiers in packages can be examined by adding *-v* to *rpm*(8) dependency
queries, eg. *rpm -qv --requires*. Dependencies added by the
*rpm-dependency-generators*(7) are marked *auto* in the output.

## Boolean dependencies
RPM can process boolean expressions in
*Requires*, *Recommends*, *Suggests*, *Supplements*, *Enhances* and *Conflicts*
type dependencies. This allows for complex dynamic behavior whereas traditional
dependencies are static in nature.

Boolean expressions are always enclosed with parenthesis.
Boolean dependencies are built out of two or more dependencies (operands)
and boolean operators:

*(*_DEPENDENCY_ _OPERATOR_ _DEPENDENCY_ ...*)*

Operands can be boolean expressions themselves. They also need to be surrounded
by parenthesis. The operators *and* and *or* may be chained together repeating
the same operator with only one set of surrounding parenthesis.

The semantics of the dependencies are unchanged, but instead of checking for
one match, all names are checked and the boolean value of a match is then
aggregated over the boolean operators. For a conflict to not prevent an install
the result has to be _false_, for everything else it has to be _true_.

## Boolean operators
*and*
	Requires all operands to be fulfilled for the term to be true.

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
	term to be true.

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
name. For versionless dependencies, the a name match is sufficient. For
versioned dependencies, the ranges must overlap as well.

A match, or lack of thereof, means different things to different relation
types:
- Non-matched requires indicates an error
- Non-matched conflicts and obsoletes are ignored
- Matched conflicts indicate an error
- Matched obsoletes indicate the package should replace another

Note that there can be arbitrary number of providers for a given capability,
and individual dependencies can be resolved to different providers. This
can lead to situations where seemingly nonsensical constraints such as
*Requires: abc < 1.0* and *Requires: abc > 2.0* are simultaneously met.
The boolean operators *with* and *without* can be used to pinpoint
capabilities to certain packages to avoid such situations.

## Range overlap
Both capabilities and dependencies can include a version range, which
can either be a single _EVR_, or a range.

For most things, the match rules are obvious: capability *= 1.0-1* matches
a *< 2.0* dependency, *= 2.0* does not match *= 1.0*, and *< 2.0* does
not match *> 2.0*. There are some notable gotchas however:

- A capability _EVR_ with no release component matches _any release_ of
  that version. For example, capability *= 1.2.3* matches *>= 1.2.3-5*
- A versionless capability matches _any EVR_. For example, *Provides: abc*
  matches *Requires: abc >= 1:5.0-2*.
- Note the significance of *Epoch* on both sides: a *= 1:1.0* capability does
  not match *<= 1.0* and a *= 1.0* capability does not match *>= 1:1.0*

## Package provides
All packages provide their own _NEVR_:

_NAME_ = [_EPOCH_:]_VERSION_-_RELEASE_

Similarly, all architecture dependent packages provide an ISA
dependency which can be used for requiring an specific architecture family
among multiple providers:

_NAME_*(*_ISA_*)* = [_EPOCH_:]_VERSION_-_RELEASE_

The *%{\_isa}* macro can be used to populate the _ISA_ value in an
*rpm-spec*(5). ISA consists of two parts separated with a dash (*-*):
the name of the architecture family in *%\_\_isa\_name* and its
bitness in *%\_\_isa\_bits*.

In addition, packages provide all the capabilities that were added
either by *rpm-dependency-generators*(7) during the package build,
or manually in the *rpm-spec*(5).

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
requires-dependency by *rpmbuild*(1). Some of these capabilities are
compile-time options when building RPM, others are RPM version-dependent.

*rpmlib()* requires cannot be satisfied by other packages. Instead, they are
resolved against the capabilities supported by the RPM process performing
the installation. The *rpmlib()* features supported by the system RPM can be
inspected by running *rpm --showrc*.

# ORDERING
RPM uses the dependency information in packages to calculate the install
and erase ordering. The basic principle is simple: any *Requires* of a package
must be present at the time of its install and erase. In other words, if
package A requires B and C, A must be installed last of the three, and
erased first.

A dependency loop has no correct ordering, so they should be avoided when
packaging. Common techniques are splitting packages to smaller sub-packages,
using unordered qualifiers (*meta* in particular) where appropriate,
using *rpm-sysusers*(7) for managing user and group creation and rewriting
scriptlets to use *rpm-lua*(7) instead of external commands.

An otherwise equal A <-> B loop can be broken one way or the other by
an ordered qualifier (eg. *pre*, *post*, *preun* or *postun*), but these
tend to add up and then lose their efficiency.

Weak dependencies are only considered for ordering if they have ordered
qualifiers, but even then the qualifiers are stripped to avoid overruling
qualifiers from hard requires.

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
*rpm-scriptlets*(7) *rpm-design*(7)

*http://www.rpm.org/*
