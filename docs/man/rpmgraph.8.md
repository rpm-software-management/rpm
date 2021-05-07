---
date: 30 June 2002
section: 8
title: RPMGRAPH
---

NAME
====

rpmgraph - Display RPM Package Dependency Graph

SYNOPSIS
========

**rpmgraph** *PACKAGE\_FILE \...*

DESCRIPTION
===========

**rpmgraph** uses *PACKAGE\_FILE* arguments to generate a package
dependency graph. Each *PACKAGE\_FILE* argument is read and added to an
rpm transaction set. The elements of the transaction set are partially
ordered using a topological sort. The partially ordered elements are
then printed to standard output.

Nodes in the dependency graph are package names, and edges in the
directed graph point to the parent of each node. The parent node is
defined as the last predecessor of a package when partially ordered
using the package dependencies as a relation. That means that the parent
of a given package is the package\'s last prerequisite.

The output is in **dot**(1) directed graph format, and can be displayed
or printed using the **dotty** graph editor from the **graphviz**
package. There are no **rpmgraph** specific options, only common **rpm**
options. See the **rpmgraph** usage message for what is currently
implemented.

SEE ALSO
========

**dot**(1),

**dotty**(1),

** http://www.graphviz.org/ \<URL:http://www.graphviz.org/\>**

AUTHORS
=======

Jeff Johnson \<jbj\@redhat.com\>
