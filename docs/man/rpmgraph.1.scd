RPMGRAPH(1)

# NAME
rpmgraph - Generate an RPM Package Dependency Graph

# SYNOPSIS
*rpmgraph* [options] _PACKAGE_FILE_ ...

# DESCRIPTION
*rpmgraph* reads *rpm* packages passed as arguments, sorts them as
for installation with *rpm*(8), and produces a dependency graph on
the standard output.

Nodes in the dependency graph are package names, and edges in the
directed graph point to the parent of each node. The parent node is
defined as the last predecessor of a package when partially ordered
using the package dependencies as a relation. That means that the parent
of a given package is the package's last prerequisite.

The output is in *dot*(1) directed graph format, and can be displayed
or printed using the *dotty*(1) graph editor from the *Graphviz*
project.

# ARGUMENTS
_PACKAGE_FILE_
	Either an *rpm* package or an *rpm-manifest*(5) file.

# OPTIONS
See *rpm-common*(8) for the options common to all rpm commands.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# SEE ALSO
*dot*(1), *dotty*(1)

*http://www.graphviz.org/*
