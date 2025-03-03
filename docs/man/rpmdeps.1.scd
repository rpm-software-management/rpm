RPMDEPS(1)

# NAME

rpmdeps - Generate RPM Package Dependencies

# SYNOPSIS

*rpmdeps* {*-P*|*--provides*} {*-R*|*--requires*} {*--rpmfcdebug*} [_FILE_ ...]

# DESCRIPTION

*rpmdeps* generates package dependencies for the set of _FILE_
arguments. Each _FILE_ argument is searched for Elf32/Elf64, script
interpreter, or per-script dependencies, and the dependencies are
printed to stdout.

# OPTIONS

*-P, --provides*
	Print the provides

*-R, --requires*
	Print the requires

*--recommends*
	Print the recommends

*--suggests*
	Print the suggests

*--supplements*
	Print the supplements

*--enhances*
	Print the enhances

*--conflicts*
	Print the conflicts

*--obsoletes*
	Print the obsoletes

*--alldeps*
	Print all the dependencies

# SEE ALSO

*rpm*(8), *rpmbuild*(1)
