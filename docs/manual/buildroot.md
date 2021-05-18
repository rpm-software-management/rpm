---
layout: default
title: rpm.org - Using a build root
---
# Using a build root

The build root is very similar to Root: (which is now legacy).
By using Buildroot: in your spec file you are indicating
that your package can be built (installed into and packaged from)
a user-definable directory.  This helps package building by normal
users.

## The Spec File

Simply use
```
  Buildroot: <dir>
```

in your spec file.  The actual buildroot used by RPM during the
build will be available to you (and your %prep, %build, and %install
sections) as the environment variable RPM_BUILD_ROOT.  You must
make sure that the files for the package are installed into the
proper buildroot.  As with Root:, the files listed in the %files
section should *not* contain the buildroot.  For example, the
following hypothetical spec file:

```
  Name: foo
  ...
  Root: /tmp
  
  %prep
  ...
  
  %build
  ...
  
  %install
  install -m755 fooprog /tmp/usr/bin/fooprog
  
  %files
  /usr/bin/fooprog
```

would be changed to:

```
  Name: foo
  ...
  BuildRoot: /tmp
  
  %prep
  ...
  
  %build
  ...
  
  %install
  install -m755 fooprog $RPM_BUILD_ROOT/usr/bin/fooprog
  
  %files
  /usr/bin/fooprog
```

## Building With a Build Root

RPM will use the buildroot listed in the spec file as the default
buildroot.  There are two ways to override this.  First, you can
have "buildroot: <dir>" in your rpmrc.  Second, you can override
the default, and any entry in an rpmrc by using "--buildroot <dir>"
on the RPM command line.

## Caveats using Build Roots

Care should be taken when using buildroots that the install directory
is owned by the correct package.  For example the file

```
	/usr/lib/perl5/site_perl/MD5.pm
```

is installed by the package perl-MD5.  If we were to use a buildroot
and specified 

```
	%files  
	/usr/lib/perl5/site_perl
```

we would end up with the directory /usr/lib/perl5/site_perl being
owned by the library package. This directory is in fact used by ALL
perl libraries and should be owned by the package for perl not any of
its libraries. It is important that the %files command specifies all
the known directories explicitly. So this would be preferable:

```
	/usr/lib/perl5/site_perl/*
```

Since we only want the files and directories that the package perl-MD5
installed into /usr/lib/perl5/site_perl/ to be owned by the package.
The directory /usr/lib/perl5/site_perl/ is created when perl is
installed.

If we were to use the bad %files line shown above, then when the MD5
package is removed, RPM will try to remove each of the perl-MD5 files and
then try to remove the dir itself. If there's still files in the
site_perl directory (e.g. from other packages) then the Unix rmdir(2)
will fail and you will get a non-zero return code from RPM. If the
rmdir succeeds then you will no longer have a site_perl directory on
your machine even though this directory was created when Perl was
installed.

The other common problem is that two packages could install two files
with the the same name into the same directory. This would lead to
other collision problems when removing the file. Care should be taken
by the packager to ensure that all packages install unique files.
Explicit use of %files can help make the packager aware of potential
problems before they happen. When you try to install a package which
contains file names already used by other packages on the system then
RPM will warn you of the problem and give a fatal error. This error can
be overridden with --force and the installed file will be replaced by the
new file and when the new package is removed the file will be removed as well.
