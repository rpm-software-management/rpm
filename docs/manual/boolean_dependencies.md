---
layout: default
title: rpm.org - Boolean Dependencies
---
## Boolean Dependencies

Starting with rpm-4.13, RPM is able to process boolean expressions in all dependencies (Requires, Recommends, Suggests, Supplements, Enhances, Conflicts). Boolean Expressions are always enclosed with parenthesis. They are build out of "normal" dependencies: either name only or name, comparison and version description.

## Boolean Operators

The following operators were introduced in RPM 4.13:

 * `and` - requires all operands to be fulfilled for the term to be True.
  * `Conflicts: (pkgA and pkgB)`
 * `or` - requires one of the operands to be fulfilled
  * `Requires: (pkgA >= 3.2 or pkgB)`
 * `if` - requires the first operand to be fulfilled if the second is (reverse implication)
  * `Recommends: (myPkg-langCZ if langsupportCZ)`
 * `if else` - same as above but requires the third operand to be fulfilled if the second is not
  * `Requires: (myPkg-backend-mariaDB if mariaDB else sqlite)`

The following additional operators were introduced in RPM 4.14:

 * `with` - requires all operands to be fulfilled by the same package for the term to be True.
  * `Requires: (pkgA-foo with pkgA-bar)`
 * `without` - requires a single package that satisfies the first operand but not the second (set subtraction)
  * `Requires: (pkgA-foo without pkgA-bar)`
 * `unless` - requires the first operand to be fulfilled if the second is not (reverse negative implication)
  * `Conflicts: (myPkg-driverA unless driverB)`
 * `unless else` - same as above but requires the third operand to be fulfilled if the second is
  * `Conflicts: (myPkg-backend-SDL1 unless myPkg-backend-SDL2 else SDL2)`

The `if` operator cannot be used in the same context with `or` and the `unless` operator cannot be used in the same context with `and`.

## Nesting 

Operands can be Boolean Expressions themselves. They also need to be surrounded by parenthesis. `and` and `or` operators may be chained together repeating the same operator with only one set of surrounding parenthesis.

Examples:

`Requires: (pkgA or pkgB or pkgC)`

`Requires: (pkgA or (pkgB and pkgC))`

`Supplements: (foo and (lang-support-cz or lang-support-all))`

`Requires: ((pkgA with capB) or (pkgB without capA))`

`Supplements: ((driverA and driverA-tools) unless driverB)`

`Recommends: ((myPkg-langCZ and (font1-langCZ or font2-langCZ)) if langsupportCZ)`

## Semantics

The Semantic of the dependencies stay unchanged but instead checking for one match all names are checked and the Boolean value of there being a match is then aggregated over the Boolean operators. For a conflict to not prevent an install the result has to be False for everything else it has to be True.

Note that '''Provides''' are not dependencies and '''cannot contain Boolean Expressions'''.

### Incompatible nesting of operators

Note that the `if` operator is also returning a Boolean value. This is close to what the intuitive reading in most cases. E.g:

`Requires: (pkgA if pkgB)` 

is True (and everything is OK) if pkgB is not installed. But if the same term is used where the "default" is False things become complicated:


`Conflicts: (pkgA if pkgB)` 

is a Conflict unless pkgB is installed and pkgA is not. So one might rather want to use

`Conflicts: (pkgA and pkgB)`

 in this case. The same is true if the `if` operator is nested in `or` terms.

`Requires: ((pkgA if pkgB) or pkgC or pkg)` 

As the `if` term is True if pkgB is not installed this also makes the whole term True. If pkgA only helps if pkgB is installed you should use `and` instead:

`Requires: ((pkgA and pkgB) or pkgC or pkg)` 

To avoid this confusion rpm refuses nesting `if` in such `or` like contexts (including `Conflicts:`) and `unless` in `and` like contexts.
