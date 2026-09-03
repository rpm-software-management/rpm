RPMINSPECTOR(1)

# NAME
rpminspector - Extract raw sections from RPM packages

# SYNOPSIS
*rpminspector* [options] _PACKAGE_FILE_

# DESCRIPTION
*rpminspector* extracts individual package sections in raw bits.

When a single section is selected, it is written to standard output.
On multiple or no specified sections, *--outdir* option is required
and the individual sections are written to separate files.

If no section gets specified, all sections are written to the given directory.

# ARGUMENTS
_PACKAGE_FILE_
	An RPM package file.

# OPTIONS
*-l*, *--lead*
	Extract the lead section.

*-s*, *--signature*
	Extract the signature header section with padding.

*-m*, *--mheader*
	Extract the main header section.

*-p*, *--payload*
	Extract the payload section.

*-o*, *--outdir*=_DIR_
	Write extracted sections as individual files in _DIR_.
	If the specified directory doesn't exist, it gets created.

See *rpm-common*(8) for the options common to all *rpm* executables.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpminspector --lead foo-1.0-1.x86_64.rpm | xxd | head*
	Hex dump of the package lead section.

*rpminspector --outdir /tmp/parts foo-1.0-1.x86_64.rpm*
	Extract all sections as separate files into _/tmp/parts/_.

*rpminspector -l -s --outdir /tmp/parts foo-1.0-1.x86_64.rpm*
	Extract the lead and signature sections.

# SEE ALSO
*rpm*(8), *rpm2archive*(1), *rpm-common*(8)
