RPMSORT(1)

# NAME
rpmsort - Sort input by RPM Package Manager (RPM) versioning

# SYNOPSIS
*rpmsort* [ _FILE_ ... ]

# DESCRIPTION
*rpmsort*(1) sorts the content of the input files, and writes a sorted list
to standard out - like *sort*(1), but aware of RPM versioning.

# ARGUMENTS
_FILE_
	A text-file containing lines to sort. If _FILE_ is omitted or *-*
	given, data is read from standard in and written to standard out.

# OPTIONS
*rpmsort* has no options.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
*echo -e 'rpm-4.18.0-3.fc38.x86_64\\nrpm-4.18.0-1.fc38.x86_64' | rpmsort*
	```
	rpm-4.18.0-1.fc38.x86_64
	rpm-4.18.0-3.fc38.x86_64
	```

# SEE ALSO
*rpm-version*(7)
