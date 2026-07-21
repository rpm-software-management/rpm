RPMPART(1)

# NAME
rpmpart - Extract raw sections from RPM packages

# SYNOPSIS
*rpmpart* [options] _PACKAGE_FILE_ ...

# DESCRIPTION
*rpmpart* extracts individual package sections in raw bits.

When a single section is selected, it is written to standard output.
On multiple or no specified sections, the individual sections are
written to separate files in the working directory.

If no section gets specified, all sections are written to the working directory.

# ARGUMENTS
_PACKAGE_FILE_
	One or more RPM package files.

# OPTIONS
*-L*, *--lead*
	Extract the lead section.

*-S*, *--signature*
	Extract the signature header section with padding.

*-H*, *--header*
	Extract the header section.

*-m*, *--metadata*
	Extract the combined metadata (lead+signature+header) as a single blob.

*-p*, *--payload*
	Extract the payload section.

*-o*, *--outdir*=_DIR_
	Write extracted sections to a specified _DIR_, overriding the default behavior.
	By default, extracted sections are dumped in working directory.
	The directory is created with all parent directories if they don't exist.

See *rpm-common*(8) for the options common to all *rpm* executables.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*rpmpart -L foo-1.0-1.x86_64.rpm | xxd | head*
	Hex dump of the package lead section.

*rpmpart foo-1.0-1.x86_64.rpm*
	Extract all sections as separate files into the current directory.

*rpmpart -o /tmp/parts foo-1.0-1.x86_64.rpm*
	Extract all sections as separate files into _/tmp/parts/_.

*rpmpart -LS -o /tmp/parts foo-1.0-1.x86_64.rpm*
	Extract the lead and signature sections.

# SEE ALSO
*rpm*(8), *rpm2archive*(1), *rpm-common*(8)
