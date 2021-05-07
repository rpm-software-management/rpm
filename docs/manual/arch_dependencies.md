---
layout: default
title: rpm.org - Architecture-specific Dependencies
---


# Architecture-specific Dependencies

On multiarch systems such as x86_64 it would be often desireable to express that a package of compatible architecture is
needed to satisfy a dependency. In most of the cases this is already handled by the automatically extracted soname dependencies,
but this is not always the case: sometimes it's necessary to disable the automatic dependency generation, and then there
are cases where the information cannot be automatically generated, such as -devel package dependencies on other -devel packages
and build dependencies. Consider the following:

```
Name: foo
...
BuildRequires: libbar-devel >= 2.2

%package devel
Requires: libbar-devel >= 2.2
...
```

This works fine on single-arch systems such as i386, but it's not sufficient on multiarch systems: when
building a 32bit package on a 64bit system, a 32bit version of the libbar-devel would be needed, but the above lets 
libbar-devel.x86_64 satisfy the build dependency too, leading to obscure build failure. Similarly
a 32bit libbar-devel would incorrectly satisfy the dependency for a 64bit package.

## ISA Dependencies

In rpm 4.6.0, the concept of ISA (Instruction Set Architecture) was introduced to permit
differentiating between 32- and 64-bit versions without resorting to file dependencies on obscure and/or 
library-version dependent paths. To declare a dependency on a package name architecture specific, 
append %{?_isa} to the dependency name, eg

```
Requires: libbar-devel%{?_isa} >= 2.2
```

This will expand to libbar-devel(archfamily-bitness) depending on the build target architecture,
for example a native build on x86_64 would give

```
Requires: libbar-devel(x86-64) >= 2.2
```

but with --target i386 (or i586, i686 etc) it would become

```
Requires: libbar-devel(x86-32) >= 2.2
```

Note that this requires all the involved packages must have been built with rpm >= 4.6.0,
older versions do not add the necessary name(isa) provides to packages.

## Anatomy of ISA macros

The ISA of a system in rpm consists of two parts: the generic
architecture family, and the bitness of the architecture. These are declared in the platform
specific macro files: %{!__isa_name} holds the architecture family and %{!__isa_bits} is the bitness.
The %{_isa} macro is just a convenience wrapper to format this to "(arch-bits)", and using the conditional
format %{?_isa} allows for backwards compatible use in spec files.

Besides the common-case use of just "Requires: foo%{_isa} >= 1.2.3", this two-part scheme permits
some rare special cases such as 64bit package also requiring 32bit version of the same architecture family:

```
Requires: foo%{_isa}
%if %{__isa_bits} == 64
Requires: foo(%{__isa_name}-32)
%endif
```

Note that there are systems with 64bit ISA which are not multiarch, so simply testing for %{!__isa_bits} in
the above example is not correct in all cases.

## Rationale

The ISA-scheme is not what people typically think of when speaking of architecture specific dependencies, instead
the general expectation is something like:

```
Requires: %{name}.%{_target_cpu}
```

While this would appear to be an obvious choice, it's not as useful as it initially seems: many architecture families
have several "sub-architectures" (such as i386, i586 and i686) and it's sometimes desirable to offer more than one
version of a package, optimized for a different architecture. For example, a package might come in i386 and i686 flavors.
You'll want the best version for a given system, but if some other package had hardwired the literal %{_target_cpu} in a
dependency, this could cause a) suboptimal package to be pulled in b) failure to install the package at all. "But make
it a regular expression" doesn't fly either: you would end up with specs full of constructs like

```
%ifarch %ix86
Requires: %{name}.(i?86|athlon|geode)
%endif
%ifarch x86_64 amd64 ia32e
Requires: %{name}.(x86_64|amd64|ia32e)
%endif
...
```

So why not just use the "base arch", ie %{_arch} for it? The problem with this is that eg. i386 is already overloaded
with meanings: it is used to mean the x86-32 architecture family, and also the physical i386 CPU. So if you had
i386 and i686 flavors of a package, the "base architecture" of the i686 variant would be i386, and ... oops, it's
not possible to tell whether it means an actual i386 CPU or i386-compatible architecture. The ISA naming scheme avoids
this ambiguity: ISA always means architecture family, not any specific architecture flavor.

The other important aspect of the implementation is compatibility: by adding the ISA information as additional
regular provides, packages remain 100% compatible with older rpm versions and depsolvers such as yum and urpmi without
any code changes. This wouldn't have been the case if the implementation relied on %{arch} tag from headers. Also
the conditional macro syntax permits spec files to be backwards compatible: when built with an rpm without ISA support,
it'll just fall back to former behavior.

It's possible that the expected "Requires: %{name}.%{_target_cpu}" style dependencies become supported too at some point,
but the need for this is rare and can be already accomplished by adding manual arch-specific provides+requires to the
packages that need it (or by arranging arch-specific filename(s) in packages and depending on those).
