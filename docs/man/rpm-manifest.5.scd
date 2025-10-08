RPM-MANIFEST(5)

# NAME
*rpm-manifest* - rpm package manifest file format

# SYNOPSIS
_GLOB_ ...

\# _COMMENT_

# DESCRIPTION
RPM package manifest files are plaintext files containing one or
more *glob*(7) entries that are expected to match either RPM package files
or other package manifests.

Package manifests can be used as a way to document installation of large
package sets in an RPM-native way, without having to set up repositories
and configure external dependency resolver programs to use them.
For end-user purposes manifests are clumsy, but for example to quickly
install a base image from a pre-determined set of package versions from
local directory, a manifest may well be easier and faster than using
a depsolver for the task. A manifest can be also used to work around
command line length limitations.

Relative globs are interpreted relative to the user's current directory.
Multiple globs per line are permitted, but for readability it's
recommended to use one per line instead.

Lines beginning with *#* are considered comments. Empty lines and
lines consisting of only whitespace are ignored.

# EXAMPLES

## Example 1. Manifest of all myproj-1.0 releases and their dependencies
```
/mnt/myproj/rpm/myproj-1.0-*.x86_64.rpm
/mnt/deplib/rpm/mylib-2.1-*.x86_64.rpm
```

## Example 2. Manifest of all noarch packages in the default rpmbuild(1) location
```
~/rpmbuild/RPMS/noarch/*.rpm
```

## Example 3. Manifest as a simple base image

Create a manifest called *mymanifest.mft* of currently installed packages,
assuming a base path of */mnt/Packages* for the RPM package files, and install
it to an alternative system root at */srv/test*:

```
rpm -qa --qf '/mnt/Packages/%{nevra}.rpm\\n' > mymanifest.mft
rpm -Uv --root /srv/test mymanifest.mft
```

# SEE ALSO
*rpm*(8) *rpm-common*(8)
