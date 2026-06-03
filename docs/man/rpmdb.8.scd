RPMDB(8)

# NAME
rpmdb - RPM Database Tool

# SYNOPSIS
## Maintenance
*rpmdb* [options] {*--initdb*|*--rebuilddb*|*--parkdb*}

## Diagnostics
*rpmdb* [options] {*--verifydb*}

## Migration
*rpmdb* [options] {*--exportdb*|*--importdb*}

# DESCRIPTION
The *rpmdb* tool is used for low-level *rpm* database operations.

# OPERATIONS
## Maintenance
*--initdb*
	Create a new database if one doesn't already exist. An
	existing database is not overwritten.

*--rebuilddb*
	Rebuild the database from the installed package headers. Discards any
	unreadable (corrupt) headers and compacts the database in case it has
	grown large over time.

	Can also be used to convert between different *rpmdb* formats.

*--parkdb*
	Park the database. Prepares and optimizes the database for inclusion on
	read-only media, such as immutable OS images.

	Write operations, such as RPM transactions, will unpark the database and
	continue normally, leaving the database unparked when finished. Thus, to
	unpark manually, use *--rebuilddb*.

	Depending on the backend used, this operation may include the removal of
	auxiliary, backend-specific files from disk, such as the write-ahead log
	or shared memory index, and thus facilitate bit-for-bit reproducible OS
	images.

## Diagnostics
*--verifydb*
	Perform a low-level integrity check on the database.
	Rarely useful, for system health *rpm --verify -a* is a far more
	meaningful operation.

## Migration
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
	Rebuild the system database.

*rpmdb --parkdb*
	Park the system database prior to inclusion on read-only media.

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
