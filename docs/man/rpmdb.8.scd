RPMDB(8)

# NAME
rpmdb - RPM Database Tool

# SYNOPSIS
*rpmdb* [options] {*--initdb*|*--rebuilddb*}

*rpmdb* [options] {*--verifydb*}

*rpmdb* [options] {*--exportdb*|*--importdb*}

# DESCRIPTION
The *rpmdb* is used for *rpm* database maintenance operations.

# OPERATIONS
*--initdb*
	Create a new database if one doesn't already exist. An
	existing database is not overwritten.

*--rebuilddb*
	Rebuild database from the installed package headers.
	Rebuilding discards any unreadable (corrupt) headers
	from the database, but also compacts the database in
	case it has grown large over time.

	Can also be used to convert between different *rpmdb*
	formats.

*--verifydb*
	Perform a low-level integrity check on the database.
	Rarely useful, for system health *rpm --verify -a* is a far more
	meaningful operation.

*--exportdb*
	Export the database in header-list format, suitable
	for transporting to another host or database type.
	The data is output to standard output.

*--importdb*
	Imports a database from a header-list format as created
	by *--exportdb*. The data is read from standard input.

# OPTIONS
See *rpm-common*(8) for the options common to all *rpm* executables.

# CONFIGURATION
There are several configurables affecting the rpmdb operations,
but in particular the following (see *rpm-config*(5) for details):
- *%\_dbpath*
- *%\_db_backend*

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmdb --initdb --dbpath /tmp/testdb*
	Initialize a new database in _/tmp/testdb_ directory.

*rpmdb --rebuilddb*
	Rebuild the system rpmdb.

*rpmdb --verifydb --root /mnt*
	Verify the system database of a system image mounted
	at _/mnt_.

*rpmdb --exportdb > /tmp/headers*
	Export the system database to _/tmp/headers_ file.

*rpmdb --importdb --define "\_db\_backend ndb" --dbpath /tmp/newdb < /tmp/headers*
	Import contents of _/tmp/headers_ header list to a (new)
	*ndb*-format database in _/tmp/newdb_.

# OPTIONS
See *rpm-common*(8) for the options common to all operations.

# SEE ALSO
*popt*(3), *rpm*(8), *rpm-common*(8)

*rpmdb --help* - as *rpm*(8) supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
