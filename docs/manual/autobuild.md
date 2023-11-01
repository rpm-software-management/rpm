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
per each build system, and specs need to just declare which build system
they use.

## Spec usage

In the most simple case, a spec will have an autobuild declaration such
as `Autobuild: cmake` and no manually specified build scripts at all.
However it's common to have to manually tweak a thing or two. There are
several ways how to accomplish this, what's appropriate depends on the
case.

1) Use append and/or prepend as necessary. Need to delete a file after
the install step? Add a `%install -a` section with the appropriate `rm`.
Need to `sed` something just before the build step? Add a `%build -p`
section with the necessary steps.

2) Another very common need is to to override the build configuration, and
this is why `%conf` is a separate section from the rest of the build, eg:

```
%conf
%configure --enable-experimental
```

However as the autobuild macros could rely on state from a previous step,
it's better to use the corresponding macro alias instead (XXX not yet
implemented in this draft):
```
%conf
%autobuild_conf --enable-experimental
```

3) Complex packages can have things like multiple build systems, in
which case you might want to invoke the macros manually, eg.

```
%autobuild_autotools_build
cd python
%autobuild_python_build
```

## Supporting new build systems

Supporting new build system types is just a matter of declaring a handful
of macros, one for any relevant build scriptlets:

Scriptlet                 | Autobuild macro
-------------------------------------------
`%prep`                   | `%autobuild_name_prep`
`%conf`                   | `%autobuild_name_conf`
`%generate_buildrequires` | `%autobuild_name_generate_buildrequires`
`%build`                  | `%autobuild_name_build`
`%install`                | `%autobuild_name_install`
`%check`                  | `%autobuild_name_install`
`%clean`                  | `%autobuiod_name_clean`

Replace "name" with the buildsystem name, eg `%autobuild_cmake_build`.
When Autobuild: tag is set, these automatically populate the corresponding
spec section, unless the spec manually overrides it. 

For example, supporting the classic autotools case could be built on top
of existing helper macros:
```
%autobuild_autotools_conf %configure
%autobuild_autotools_build %make_build
%autobuild_autotools_install %make_install
```
