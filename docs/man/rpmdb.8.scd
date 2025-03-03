RPMDB(8)

# NAME

rpmdb - RPM Database Tool

# SYNOPSIS

*rpmdb* {*--initdb*|*--rebuilddb*}

*rpmdb* {*--verifydb*}

*rpmdb* {*--exportdb*|*--importdb*}

# DESCRIPTION

The general form of an rpmdb command is

*rpmdb* {*--initdb*|*--rebuilddb*} [*-v*] [*--dbpath* _DIRECTORY_] [*--root* _DIRECTORY_]

Use *--initdb* to create a new database if one doesn't already exist
(existing database is not overwritten), use *--rebuilddb* to rebuild
the database indices from the installed package headers.

*--verifydb*
	Perform a low-level integrity check on the database.

*--exportdb*
	Export the database in header-list format, suitable
	for transporting to another host or database type.

*--importdb*
	Imports a database from a header-list format as created
	by *--exportdb*.

# SEE ALSO

*popt*(3), *rpm*(8), *rpmkeys*(8), *rpmsign*(1), *rpm2cpio*(1),
*rpmbuild*(1), *rpmspec*(1)

*rpmdb --help* - as *rpm*(8) supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
