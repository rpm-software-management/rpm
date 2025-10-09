ELFDEPS(1)

# NAME
*elfdeps* - Dependency generator for ELF binaries

# SYNOPSIS
*elfdeps* <operation> [options]

*elfdeps* <operation> [options] [_FILE_] ...

# DESCRIPTION
*elfdeps* is a _dependency generator_ for ELF executables
and dynamic shared object (DSO) files. The filenames can be received
on the standard input (when used as a generator), or command line
arguments for testing convenience.

*elfdeps* encodes information stored in ELF files into a format suitable
for use in RPM dependencies. The main interest areas are *DT_SONAME*,
*DT_NEEDED* fields of the *SHT_DYNAMIC* section, and symbol versioning
information in the *SHT_GNU_verdef* and *SHT_GNU_verneed* sections.

*elfdeps* is not normally invoked directly, but doing so can be useful for
troubleshooting dependency generation and developing new dependency
generators. The installation directory of *elfdeps* can be determined with
*rpm -E "%{\_rpmconfigdir}"*.

This man page describes the options specific to this generator,
see *rpm-dependency-generators*(7) for information about the overall
mechanism.

# OPERATIONS
*-P*,
*--provides*,
	Print the provides dependencies.

*-R*,
*--requires*
	Print the requires dependencies.

# ARGUMENTS
_FILE_
	An ELF file. Other file types are silently ignored.

# OPTIONS
*--no-fake-soname*
	Do not emit a fake soname for DSOs that don't set *DT_SONAME*.
	By default, *elfdeps* generates a soname from the filename for
	DSOs that do not have one.
	Only effective when printing provides dependencies.

*--no-filter-soname*
	Do not filter sonames. By default, *elfdeps* does some sanity
	checking to filter out redundant and/or invalid soname entries.
	Only effective when printing provides dependencies.

*-m*,
*--multifile*
	Use *multifile* protocol for output.
	See *rpm-dependency-generators*(7).

*--require-interp*
	Emit a requires dependency for the ELF interpreter (*PT_INTERP*),
	if there is one.
	Only effective when printing requires dependencies.

*--soname-only*
	Emit only the soname (*DT_SONAME*) of a DSO, omitting any symbol
	versioning information.
	Only effective when printing provides dependencies.

# OUTPUT
See *Protocols* in *rpm-dependency-generators*(7).
By default, *singlefile* protocol is used, but the *multifile* protocol
can be enabled with the *--multifile* option.

# EXIT STATUS
On success, 0 is returned, a non-zero failure code otherwise.

# EXAMPLES
*elfdeps --requires /bin/ls*
	Print ELF-level requires dependencies of the _/bin/ls_ executable.

*find mylib-1.0-build/BUILDROOT --name "\*.so\*" | elfdeps -P --multifile*
	Print ELF-level provides dependencies of all shared libraries
	inside the _mylib-1.0-build/BUILDROOT_ directory, using the
	*multifile* generator protocol.

# SEE ALSO
*rpmbuild*(1) *rpm-dependency-generator*(7) *elf*(5) *readelf*(1)

*http://www.rpm.org/*
