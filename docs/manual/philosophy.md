---
layout: default
title: rpm.org - RPM's Philosophy
---
# RPM's Philosophy

RPM is a general purpose software manager. It has
two major parts: rpmbuild that combines upstream sources with a meta
data and build instructions in a "spec file" and additional files and
patches. It allows to bundle all this up into a source package and
build the software into a binary package. The second part - the rpm
utility - can then install, update or remove the packages after
checking for consistency.

RPM differs from special purpose package managers - like those
targeting a specific programming language - in trying to not make
assumptions about how software looks like or gets packaged. Packaging
software can be messy and RPM accommodates for that.

It still offers help to create a POSIX like operation system. Macros
parameterize locations to make the build instructions more generic and
more easily transferable between distributions. Nevertheless RPM
packages are targeting a specific distribution (and release thereof). They are
typically not suited to be installed or even built elsewhere without
tweaking. RPM acknowledges that different distributions have
different goals, resulting in different design decisions. The
specifics of the packages often reflect these.

RPM as a upstream project still tries to keep distributions from
diverging unnecessarily but is aware that these differences are the
reason distributions exist in the first place.

## RPM in the Software handling stack

RPM sees itself as filling specific layers in the software handling
stack. It relies on build systems like make and the upstream provided
build scripts to orchestrate the actual build and passes the acquiring
and selection of the "right" packages up to updaters like yum, dnf,
zypper, etc.

The stack typically looks like this:

 * Upstream build scripts using tools like maker, cmake, ant, ...
 * rpmbuild for running those via a Spec file
 * Build systems for installing build dependencies and keeping track of build artifacts
 * Package repositories offering binary packages
 * Installers/Updaters selecting and downloading packages
 * rpm checking for consistency and installing/updating the packages

As such RPM is only a part in a larger set of tools and some users may
never actually interact with RPM directly.

## Design goals

* General purpose software management
* Building from pristine sources
* Unattended operation
  * for building packages
  * for installing/updating/deleting packages
* Reproducibility of builds and installs
* Verifiability of packages and installed software

### Rolling out (security) updates quickly.

Getting updates installed quickly is one of the main design
goals. Many features and following design decisions are supporting
this goal.

### Packaging dependencies separately

Libraries should be packaged separately and binaries should link to
the version provided by system packages. Static linking is
discouraged. This limits the number of packages that need updates or
re-builds in case of a vulnerability.

### Unattended operation

Package installation and update is unattended and must not require
user interaction. This allows automatically installing security - and
other - updates.

Building the packages also runs unattended. This prevents user input
from changing the output but also makes rebuilding packages - e.g. for
changed dependencies - possible at large scale.

### Clear update path

Each package name creates an update path where packages with the same
name and a newer version (technically Epoch-Version-Release) are an
update for the packages with lower version. Packages are not allowed
to make assumptions on what intermediate packages were installed or
not.

### Bundle Sources, Changes and Build instructions together

Source packages bundle upstream sources, patches and build
instructions into one entity. If changes need to be made everything
needed is available, including a list of packages needed to run the
build.

#### Separate Upstream Source from Patches

Source packages are supposed to contain the unaltered upstream sources
in a way that their checksum can be checked. All changes can be done
by applying patches or running build scripts. This makes it easy to
understand what the packager did to the software. This is important to
be able to figure out where issues arise from and to see which issues
are fixed in the package - even if the upstream version is still
vulnerable.

#### Support back porting fixes

A distribution often has a different approach to fixing software from an
upstream project. Upstream projects are often content with fixing
their latest release(s). Distribution often support a particular
version of each software and need those to be fixed. The Sources and
Patches approach makes handling this a lot easier (although it is
lately being extended with the use of version control like git). 

### Allow 3rd party packages

Although RPM is designed to package a whole distribution it explicitly
supports 3rd parties to also provide packages without becoming part of
the distribution and their processes.

Rpmbuild can be run locally without the use of a big build
system. Dependencies are in large part automatic and target libraries
inside the packages instead of relying on conventions like package
names. This way 3rd party packages can be checked for compatibility
beyond them just claiming that they target the distribution in question.

For proprietary softerware nosource packages allow to not ship the
sources but only the build instructions to keep proprietary source
private. They still can be used to generate binary packages in
combination with the sources.

### Handling all system files

RPM takes ownership of all system files and their life cycle. While
packages can copy files in their scriptlets at installation time this
is strongly discouraged. Even files that are not actually shipped can
be added to the package as %ghost files to allow RPM to handle
them. All system files and directories should be owned by a package.

Scriptlets should be used sparingly. Most use cases - like updating
caches and indexes - can be dealt with by using a central
filetrigger. Although these files may get altered they still should be
owned by a package.

### Verify files

RPM keeps checksums of all files it maintains. Packages have checksums
and can and should be signed. Files that need to change on disk - like
config files, databases or indexes - can be marked and are treated
differently when being changed. While unchange config files may be
replaced changed ones will be retained or backed up - depending on the
way they have been packaged.

## Levels of control

Most things in RPM can be configured by macros. This allows different
actors to set them and overwrite what was set previously.

Most basic are the defaults delivered by RPM upstream. Distributions
are able to overwrite them by patching or providing new macro
files. Packages can also ship macro files for other packages to use as
BuildRequires. Finally most build related macros can also be
overwritten in the Spec file - giving the packager the last word.

There are a few command line options to allow users to influence how
packages are installed but their use is discouraged for the most part
and packages should be installed as they are.
