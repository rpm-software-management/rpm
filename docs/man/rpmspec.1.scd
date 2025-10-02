RPMSPEC(1)

# NAME
rpmspec - RPM Spec Tool

# SYNOPSIS
*rpmspec* {*-q*|*--query*} [options] [query-options] _SPEC_FILE_ ...

*rpmspec* {*-P*|*--parse*} [options] _SPEC_FILE_ ...

*rpmspec* *--shell* [options] [_SPEC_FILE_ ...]

# DESCRIPTION
*rpmspec* is a tool for querying a spec file. More specifically for
querying hypothetical packages which would be created from the given
spec file. So querying a spec file with *rpmspec* is similar to
querying a package built from that spec file. But it is not identical.
With *rpmspec* you can't query all fields which you can query from a
built package. E. g. you can't query BUILDTIME with *rpmspec* for
obvious reasons. You also cannot query other fields automatically
generated during a build of a package like auto generated dependencies.

# OPERATIONS
*-q*,
*--query*
	Query parsed spec header, similarly to querying packages.

*-P*,
*--parse*
	Output parsed spec file to the standard output. That is,
	conditionals are handled, macros parsed and so on.

*--shell*
	Invoke an interactive shell for inspecting macros, optionally
	after parsing a spec. Mainly useful for troubleshooting.

# ARGUMENTS
_SPECFILE_
	RPM spec file used for building packages with rpmbuild.

# OPTIONS
See *rpm-common*(8) for the options common to all operations.

# QUERY OPTIONS
*--qf* _QUERYFMT_,
*--queryformat* _QUERYFMT_
	Specify output format for spec queries. See *rpm-queryformat*(7) for
	details.

*--rpms*
	Operate on the all binary package headers generated from spec.

*--builtrpms*
	Operate only on the binary package headers of packages which would be
	built from spec. That means ignoring package headers of packages that
	won't be built from spec i. e. ignoring package headers of packages
	without file section.

*--srpm*
	Operate on the source package header(s) generated from spec.

# ENVIRONMENT
See *rpm-common*(8).

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmspec -q rpm.spec*
	Get list of binary packages which would be generated from the rpm
	spec file, eg:
	```
	rpm-4.11.3-3.fc20.x86_64
	rpm-libs-4.11.3-3.fc20.x86_64
	rpm-build-libs-4.11.3-3.fc20.x86_64
	...
	```

*rpmspec -q --qf "%{name}: %{summary}n" rpm.spec*
	Get summary infos for single binary packages generated from the
	rpm spec file, eg:
	```
	rpm: The RPM package management system
	rpm-libs: Libraries for manipulating RPM packages
	rpm-build-libs: Libraries for building and signing RPM packages
	...
	```

*rpmspec -q --srpm rpm.spec*
	Get the source package which would be generated from the rpm spec
	file, eg:
	```
	rpm-4.11.3-3.fc20.x86_64
	```

*rpmspec -P rpm.spec*
	Parse the rpm spec file to stdout, eg:
	```
	Summary: The RPM package management system
	Name: rpm
	Version: 4.14.0
	...
	```

*rpmspec --shell*
	Run interactive macro shell for debugging macros, eg:
	```
	> %define foo bar
	> %foo
	bar
	> %(date)
	Tue Apr 13 03:55:37 PM EEST 2021
	> %getncpus
	8
	```

*rpmspec --shell popt.spec*
	Run interactive macros shell in spec context, eg:
	```
	%name
	popt
	%version
	1.18
	```

# SEE ALSO
*popt*(3), *rpm*(8), *rpmbuild*(1), *rpm-queryformat*(7), *rpm-macros*(7)

*rpmspec --help* - as rpm supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
