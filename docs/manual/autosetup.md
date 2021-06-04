---
layout: default
title: rpm.org - Automating patch application in specs
---
# Automating patch application in specs

## %autosetup description

Starting with version 4.11, RPM has a set of new macros to further
automatesource unpacking and patch application. Previously one had to
manually specify each patch to be applied, eg

```
%prep
%setup -q
%patch0
%patch1
%patch2
...
%patch149
```

This can get rather tedious when the number of patches is large. The new
`%autosetup` macro allows taking care of all this in a single step: the
following applies all the patches declared in the spec, ordered by the patch
number:

```
%prep
%autosetup
```

In addition to automating plain old `patch` command invocations, `%autosetup`
allows utilizing various version control systems such as git, mercurial (aka
hg), quilt and bzr for managing the build directory source. For example this
unpacks the vanilla source, initializes a git repository in the build
directory and then applies all the patches defined in the spec using
individual git apply + commits:

```
%autosetup -S git
```

The resulting build directory can be used for bisecting problems introduced
in patches, and developing new patches from the build directory is more
natural than with gendiff.

## %autosetup options

Generally `%autosetup` accepts the same arguments as %setup does. The notable
exceptions are

* `%autosetup` defaults to quiet operation, so `-q` is not needed or accepted.
  Use `-v` to enable verbose source unpacking if needed.
* `-N` disables automatic patch application if necessary for some reason. If
  `%autosetup` is called with `-N`, the patch-application phase can be
   manually invoked with `%autopatch` macro.
* `-S<vcs>` specifies the VCS to use. Currently supported VCSes are: `git`,
  `hg` (for mercurial), `bzr`, `quilt`, `patch`, `git_am` (rpm >= 4.12)
  and `gendiff` (rpm >= 4.14). If `S` is omitted, `%autosetup` defaults to
  `patch`
* `-p<number>` argument to control patch prefix stripping (same as
  `-p` to `%patch`)
* `-b` (for creating patch backups) is accepted but currently ignored -
  this is not meaningful for a full-blown VCS anyway. If you need backups
  for `gendiff` use, use `gendiff` backend.

Note that the exact behavior of `-S` option depends on the used VCS: for
example quilt only controls patches whereas git and mercurial control the
entire source repository.

## %autopatch

Sometimes you need more control than just "apply all", in which case you
can call `%autopatch` directly. By default it simply applies all patches
in the order declared in the spec, but you can additionally control the
range with options, or pass patch numbers as arguments.  The supported
options are

* `-v` verbose operation
* `-p<number>` argument to control patch prefix stripping (same as
  `-p` to `%patch`, normally passed down from `%autosetup`)
* `-m<number>` Apply patches starting from `<number>` range
* `-M<number>` Apply patches up to `<number>` range

Some examples:

# Apply patches with number >= 100
`%autopatch -m 100`
# Apply patches with number <= 400
`%autopatch -M 400`
# Apply patches 80 to 99
`%autopatch -m 80 -99`
# Apply patches 1, 4 and 6
`%autopatch 1 4 6`

## Automating patch (and source) declarations

While typically patch and source names tend to be descriptive for humans,
making automating the declarations impossible, some upstreams (for example
bash and vim) provide bugfixes by serially numbered patches. In such cases
automation can be taken one step further by programmatically generating the
patch declarations as well. As of this writing there are no specific helper
macros for performing this, but for example the embedded Lua interpreter can
be used for the purpose:

```
%{lua:for i=1,45 do print(string.format("Patch%u: bash42-%03u\n", i, i)) end}
```


On spec parse, the above expands to as many patch declarations (best
inspected with `rpmspec --parse <spec>`):

```
Patch1: bash42-001
Patch2: bash42-002
Patch3: bash42-003
Patch4: bash42-004
...
Patch45: bash42-045
```

Combined with `%autosetup`, this can eliminate a very large number of
repetitive spec lines, making package maintenance that little bit easier. 
