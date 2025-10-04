RPM2ARCHIVE(1)

# NAME
rpm2archive - Create tar or cpio archive from RPM Package Manager (RPM) package

# SYNOPSIS
*rpm2archive* [options] [_PACKAGE_FILE_] ...

# DESCRIPTION
*rpm2archive* converts RPM package files to other archive formats.

If the standard output is a regular file or a pipe, the archive is written
to the standard output. If the standard output is a terminal, the output
is written to a file by the same name, appended with a _.tgz_ or _.cpio.gz_
suffix when compressed or _.tar_ or _.cpio_ otherwise depending on the format.
The output is compressed in the *gzip*(1) format by default.

*rpm2archive* does not verify package-level signatures or checksums, but
it does verify the per-file checksums.

Supports RPM package formats 3, 4 and 6.

# ARGUMENTS
_PACKAGE_FILE_
	A binary or source RPM package.

If no arguments are present, or a dash (*-*) is given as an argument,
data is read from the standard input.

# OPTIONS
*-n*, *--nocompression*
	Generate an uncompressed archive. If in the file creation mode,
	use _.tar_ as postfix of the filename.

*-f*, *--format*=<_FORMAT_>
	Generate archive in specified _FORMAT_:
	- *pax*: Restricted Portable Archive Exchange as used by *tar*(1)
	  (default)
	- *cpio*: The "new" ASCII *cpio*(5) format. Note that this format
	  has a 4GB limit on the individual file size.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpm2archive glint-1.0-1.i386.rpm | tar -xvz*
	Extract _glint-1.0-1.i386.rpm_ package contents with *tar*(1).

*rpm2archive --nocompression --format=cpio glint-1.0-1.i386.rpm | cpio -idv*
	Extract _glint-1.0-1.i386.rpm_ package contents as an uncompressed
	*cpio*(5), compatible with *rpm2cpio*(1) output.

*rpm2archive glint-1.0-1.i386.rpm ; tar -xvz glint-1.0-1.i386.rpm.tgz*
	Extract _glint-1.0-1.i386.rpm_ package contents into an
	archive named _glint-1.0-1.i386.rpm.tgz_ and extract the written
	archive with *tar*(1).

*cat glint-1.0-1.i386.rpm | rpm2archive - | tar -tvz*
	Feed _glint-1.0-1.i386.rpm_ package into *rpm2archive* via standard
	input, and list contents with *tar*(1).

# SEE ALSO
*rpm2cpio*(1), *rpm*(8)
