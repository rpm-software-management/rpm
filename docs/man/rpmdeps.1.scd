RPMDEPS(1)

# NAME
rpmdeps - Generate RPM Package Dependencies

# SYNOPSIS
*rpmdeps* <operation> [options]

*rpmdeps* <operation> [options] _FILE_ ...

# DESCRIPTION
*rpmdeps* generates package dependencies for one or more files, received
either as a list of filenames on the standard input, or command line
arguments.

*rpmdeps* is not normally invoked directly, but doing so can be useful for
troubleshooting dependency generation and developing new dependency generators.
The installation directory of *rpmdeps* can be determined with
*rpm -E "%{\_rpmconfigdir}"*.

Note that the actual *rpmdeps* command is only used for automatic dependency
generation in a legacy compatibility mode of *rpmbuild*(1).

# OPERATIONS
*--alldeps*
	Print all the dependencies.

*--conflicts*
	Print the conflicts dependencies.

*--enhances*
	Print the enhances dependencies.

*--obsoletes*
	Print the obsoletes dependencies.

*--orderwithrequires*
	Print the orderwithrequires dependencies.

*-P, --provides*
	Print the provides dependencies.

*--recommends*
	Print the recommends dependencies.

*-R, --requires*
	Print the requires dependencies.

*--suggests*
	Print the suggests dependencies.

*--supplements*
	Print the supplements dependencies.

# ARGUMENTS
_FILE_
	Any regular file.

# OPTIONS
See *rpm-common*(8) for the options common to all operations.

# ENVIRONMENT
*RPM_BUILD_ROOT*
	The prefix path to strip out when generating dependencies
	based on actual paths.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmdeps --requires /bin/ls*
	Output the automatic requires dependencies of the *ls*(1) binary.

*find popt-1.19-build/BUILDROOT -type f | /usr/lib/rpm/rpmdeps -P*
	Output the automatic provides of all files in the
	_popt-1.19-build/BUILDROOT_ directory

# SEE ALSO
*rpm*(8), *rpmbuild*(1), *rpm-common*(8)
