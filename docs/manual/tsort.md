---
layout: default
title: rpm.org - Package ordering
---
# Package ordering

Rpm orders transactions solely based on package dependency information:
dependencies are installed before the dependent, and erased after it.
If the install/erase order is not right, then either the dependency
information is incomplete, incorrect or there are unresolvable dependency
loops.

The order is calculated by matching dependencies on `Provides` from the
packages in the transaction. If package A `Requires` B, then a provider
of B will be installed before A, and erased after it.
In case of multiple providers, heuristics are used to pick the best match.
Weak dependencies (`Recommends`, `Suggests`, `Supplements`, `Enhances`)
and ordering hints (`OrderWithRequires`) are similarly considered, if the
provider is present in the transaction.
Note that `Supplements` and `Enhances` are reverse dependencies and are
ordered thus.`Obsoletes` and `Conflicts` dependencies are not used for
ordering.

A dependency loop is generally a packaging bug as no correct ordering is
possible to achieve in that situation. They are best solved at the source,
by splitting packages to sub-packages to eliminate the loop.
Sometimes this is not practical though, and also some dependencies are
not relevant for ordering at all. Rpm supports various
[dependency qualifiers](spec.md#Requires) to supply
context that rpm uses for loop resolution.

Qualifiers work in two ways, depending on the context and the qualifier:
they can either *resolve* the loop, or *favor* loop cutting.

Normally, `Requires: A` and `Requires(pre): A` result in exactly the same
install order but when a loop is detected, the latter is favored during
installation. During erasure, the latter is ignored.
In simple cases this is sufficient to nudge the loop resolution in the
desired direction, but there is no guarantee. Also note that favoring
is only used on hard `Requires` dependencies.

`Requires(preun): A` declares the dependency is only needed during erasure,
so it can resolve an install-time loop. Unordering qualifiers cause the
dependency to be ignored entirely during ordering, so they resolve loops.

Install-order qualifiers:
* `pre`
* `post`

Erase-order qualifiers:
* `preun`
* `postun`

Unordering qualifiers:
* `pretrans`
* `posttrans`
* `meta`
* `verify`

Packages belonging to a dependency loop are installed as a consecutive group,
using the qualifier hints to try and cut the loop with least minimal
disruption, but one should remember that there is no such thing as a
correct order for a loop.

Ordering issues and dependency loops can be difficult to analyze.
To aid with this, rpm can print out transaction ordering diagnostics.
These diagnostics can be enabled with the `--deploops` switch from the cli
or the `RPMTRANS_FLAG_DEPLOOPS` transaction flag from the API. Installing
to (and erasing from) an empty chroot is an effective way of discovering
missing dependencies.
