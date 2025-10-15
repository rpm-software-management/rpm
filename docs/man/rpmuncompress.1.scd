RPMUNCOMPRESS(1)

# NAME
*rpmuncompress* - RPM utility for compressed files and archives

# SYNOPSIS
*rpmuncompress* [options] _FILE_

*rpmuncompress* [options] [extract-options] {*-x*|*--extract*} _ARCHIVE_

# DESCRIPTION
*rpmuncompress* is a utility for transparently outputting compressed
and uncompressed files, and extracting archives.

It's normally invoked internally by *rpmbuild*(1) to handle *%setup*
and *%patch* related tasks in spec files, but can be useful manually
invoked as well.
The installation directory of *rpmuncompress* can be determined with
*rpm -E "%{\_rpmconfigdir}"*.

# ARGUMENTS
_FILE_
	A regular file, optionally compressed.

_ARCHIVE_
	A *tar*(1), *zip*(1) or Ruby GEM archive. *tar* archives may
	be compressed.

# OPERATIONS
By default, *rpmuncompress* outputs _FILE_ to standard output similar
to *cat*(1), but transparently decompressing many popular compression
formats, similar to *zcat*(1).

*-x*,
*--extract*
	Extract the _ARCHIVE_ passed as argument into the current directory,
	transparently uncompressing *tar*(1) archives if needed.

# OPTIONS
*-n*,
*--dry-run*
	Output the shell command that would be executed without this option.

See *rpm-common*(8) for options common to all rpm executables.

# EXTRACT OPTIONS
*-C* _PATH_,
*--path*=_PATH_
	Extract into to _PATH_ regardless of the archive's internal layout.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmuncompress /tmp/3911.patch.gz | patch -p1*
	Uncompress and output _/tmp/3911.patch.gz_, similar to *zcat*(1),
	and pipe to *patch*(1).

*rpmuncompress -x myproject.zip*
	Extract _myproject.zip_ archive into the current directory.

*rpmuncompress -x -C myproject myproject-1.0.7rc1.tar.gz*
	Uncompress and extract _myproject-1.0.7rc1.tar.gz_ archive into
	_myproject_ directory, regardless of it's internal structure.

# SEE ALSO
*rpm-common*(8), *rpmbuild*(1), *popt*(3)

*http://www.rpm.org/*
