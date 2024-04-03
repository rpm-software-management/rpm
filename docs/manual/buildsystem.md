---
layout: default
title: rpm.org - Declarative builds
---

# Declarative builds

Most software follows common patterns as to how it should be prepared and
built, such as the classic `./configure && make && make install` case.
In a spec, these all go to their own sections and when combined with
the steps to unpack sources, it creates a lot of boilerplate that is
practically identical across a lot of specs. With the odd tweak to
this or that detail for distro preferences and the like. To reduce
the repetitive boilerplate, rpm >= 4.20 adds support for a declarative
build system mechanism where these common steps can be defined centrally
per each build system. Packagers then only need to declare which build
system they use, and optionally supply additional switches and/or
arguments for the steps where needed.

## Spec usage

In the most simple case, a spec will have an buildsystem declaration such
as `BuildSystem: cmake` and no manually specified build scripts at all.
However it's common to have to manually tweak a thing or two. There are
several ways how to accomplish this, what's appropriate depends on the
case.

1) Use append and/or prepend as necessary. Need to delete a file after
the install step? Add a `%install -a` section with the appropriate `rm`.
Need to `sed` something just before the build step? Add a `%build -p`
section with the necessary steps.

2) Another very common need is to pass extra arguments to the build
commands, build configuration in particular. This is done with the
`BuildOption` tag, which can appear arbitrary number of times
in the spec for each section.

```
BuildOption(<section>): <option string>
```

without the parenthesis defaults to configuration. In other words,
these two lines are exactly equivalent:

```
BuildOption: --enable-fu
BuildOption(conf): --enable-fu
```

Passing these per-section options to the actual buildsystem of the
package is the responsibility of the buildsystem specific macros.

## Supporting new build systems

Supporting new build system types is just a matter of declaring a few
macros for the build scriptlet sections relevant to the build system.

Scriptlet                 | Mandatory | Buildsystem macro
--------------------------|-----------|------------------
`%prep`                   | No        | `%buildsystem_name_prep`
`%conf`                   | Yes       | `%buildsystem_name_conf`
`%generate_buildrequires` | No        | `%buildsystem_name_generate_buildrequires`
`%build`                  | Yes       | `%buildsystem_name_build`
`%install`                | Yes       | `%buildsystem_name_install`
`%check`                  | No        | `%buildsystem_name_check`
`%clean`                  | No        | `%buildsystem_name_clean`

Replace "name" with the buildsystem name, eg `%buildsystem_cmake_build`.
When `BuildSystem:` tag is set, these automatically populate the corresponding
spec section, unless the spec manually overrides it. All buildsystem
macros are required to be parametric to have enforceable semantics.

For example, supporting the classic autotools case could be built on top
of existing helper macros:
```
%buildsystem_autotools_conf() %configure %*
%buildsystem_autotools_build() %make_build %*
%buildsystem_autotools_install() %make_install %*
```

## Global defaults

While the actual steps to build and install obviously differ greatly among
different buildsystems, they still typically share a lot of common steps.
Namely, unpacking sources, applying patches and cleaning up at the end.
To avoid each buildsystem having to declare a `%prep` to automatically
perform the same common duties, with inevitable subtle differences and
bugs, rpm additionally supports global default actions for all scriptlets
supported by the buildsystem mechanism. These defaults, if defined,
are only called if there's no corresponding buildsystem specific macro
defined.

Rpm ships with a default to perform `%autosetup -p1 -C` in `%prep`,
so unless a buildsystem has an unusual source preparation sequence
source preparation will "just work". Passing extra arguments to a section
is exactly the same with defaults and buildsystem specific macros, so
the user does not need to know which one is being used. For example,
if your patches need to be applied with a non-default prefix stripping:

```
BuildOption(prep): -p0
```

Note that the defaults are only meant for upstream and distro-level
customization only, do not override them for your own purposes!
