RPM2CPIO(1)

# NAME

rpm2cpio - Extract cpio archive from RPM Package Manager (RPM) package

# SYNOPSIS

*rpm2cpio* _FILE_

# DESCRIPTION
*rpm2cpio* converts the specified *rpm* package to a *cpio*(1) archive
on the standard ouput.

*Note:* the *cpio*(5) format cannot host individual files over 4GB in size,
and so this tool is considered obsolete.  Use *rpm2archive* instead.

# ARGUMENTS
_FILE_
	An *rpm* package file. If *-* is passed, standard input is used.

# OPTIONS
*rpmcpio* has no options.

# EXIT STATUS
On success, 0 is returned, a non-zero failure code otherwise.

# EXAMPLES
*rpm2cpio glint-1.0-1.i386.rpm | cpio -dium*
	Extract _glint-1.0-1.i386.rpm_ contents relative to current
	directory.

*cat glint-1.0-1.i386.rpm | rpm2cpio - | cpio -tv*
	View _glint-1.0-1.i386.rpm_ file list with *cpio*(1).

# SEE ALSO
*cpio*(1), *rpm*(8), *rpm2archive*(1)
