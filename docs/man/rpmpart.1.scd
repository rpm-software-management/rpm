RPMPART(1)

# NAME
rpmpart - Extract raw sections from RPM packages

# SYNOPSIS
*rpmpart* [options] _PACKAGE_FILE_

# DESCRIPTION
*rpmpart* extracts individual package sections in raw bits.

When a single section is selected, it is written to standard output.
On multiple or no specified sections, *--outdir* option is required
and the individual sections are written to separate files.

If no section gets specified, all sections are written to the given directory.

# ARGUMENTS
_PACKAGE_FILE_
	An RPM package file.

# OPTIONS
*-s*, *--sections*=_LIST_
	Comma separated list of extractable sections.
	Sections available: *lead*, *signature*, *mainheader*, *payload*, *header*.
	The *header* section is a combination of lead, signature and main header.

*-o*, *--outdir*=_DIR_
	Write extracted sections to a specified _DIR_, overriding the default behavior.
	By default, extracted section are dumped in working directory.
	The directory is created with all parent directories if they don't exist.

See *rpm-common*(8) for the options common to all *rpm* executables.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmpart -s lead foo-1.0-1.x86_64.rpm | xxd | head*
	Hex dump of the package lead section.

*rpmpart foo-1.0-1.x86_64.rpm*
	Extract all sections as separate files into the current directory.

*rpmpart -o /tmp/parts foo-1.0-1.x86_64.rpm*
	Extract all sections as separate files into _/tmp/parts/_.

*rpmpart -s lead,signature -o /tmp/parts foo-1.0-1.x86_64.rpm*
	Extract the lead and signature sections.

# SEE ALSO
*rpm*(8), *rpm2archive*(1), *rpm-common*(8)
