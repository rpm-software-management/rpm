RPM-VERSION(7)

# NAME
*rpm-version* - RPM version system

# SYNOPSIS
\[_EPOCH_*:*]_VERSION_[*-*_RELEASE_]

# DESCRIPTION
A label known as _EVR_ is used to refer to software versions in RPM
consisting of one to three _components_:

- _VERSION_ reflects the actual packaged software version.
- _RELEASE_ reflects packaging revisions within that software version.
- _EPOCH_ is an artificial override to allow working around versioning
  anomalies.

All RPM packages have a _VERSION_ and a _RELEASE_. In context of an
_EVR_ label (such as in versioned dependencies), only the _VERSION_
component is mandatory however.

## Version
The _VERSION_ string reflects the actual packaged software version.

The string consists of ASCII alphanumeric characters, optionally
_segmented_ with the _separators_ period (*.*), underscore (*\_*) and
the plus sign (*+*), and _operators_ tilde (*~*) and caret (*^*).

Notably, the dash character (*-*) can NOT be used in _VERSION_ or _RELEASE_
as it is used to separate the components.

## Release
The _RELEASE_ reflects a revision within a single software version. It's
incremented whenever changes are made to a package, and should be reset
(typically to 1) whenever the software version changes.

The format is exactly the same as for _VERSION_.

## Epoch
The _EPOCH_ is a non-negative integer, separated from the version with
a colon (*:*). It's the most significant part of an
_EVR_, skewing version comparison to make an older version to appear newer.
It's sometime necessary to work around version anomalies such as software
project changing it's versioning scheme, but also packaging errors.

The epoch should be only used as a last resort. It violates the principle
of least surprise, and changing it requires all related versioned
dependencies in other packages to be updated accordingly.

## Comparing
Two _EVR_'s are compared a left to right, component at a time.
The components are compared are compared a _segment_ at a time.
A missing _EPOCH _ is equal to epoch of zero.

Consequent alphabets and numbers form _implicit segments_, and
_explicit segments_ can be introduced with _separators_ and _operators_.
Numeric segments are compared numerically as integers with leading zeros
ignored, otherwise lexicographical comparison is used.
That is, *abc123* consists of two segments: *abc* and *123* and is equal
to *abc0123*, *abc.123* and *abc.000123* despite difference in appearance.

Numeric segments are considered newer than alphabetic segments regardless
of the actual content. When otherwise equal, the component with more
segments is considered newer, and similarly an _EVR_ with more compoments
is considered newer. For example, *0.0* is newer than *0* and *1.xyz* is
older than *1.0* but newer than *1*.

The segment _separator_ characters are not compared, so they can be used
interchangeably, and multiple consecutive separators are treated as if
only one separator was used. Thus, *1.0* is equal to *1+0* and *1+.+0*

The segment _operators_ tilde (*~*) and caret (*^*) can be
used as special separators to tilt the comparison of the following segment
older or newer, respectively. The tilde operator is particularly useful for
pre-release versions (beta, release candidate etc), and the caret for
post-release versions.

# EXAMPLES
*123*
	A simple one-segment version *123*. Newer than *99*, older
	than *321*.

*1.0.1*
	A segmented version string *1.0.1*, such as commonly used in software
	projects to indicate major.minor.micro semantics. Newer than
	*1.0*, older than *1.0.2*.

*2.60.1-1*
	First release of version *2.60.1*. Newer than *2.0* or *2.60*,
	but older than *3.0*.

*1.0-5*
	Fifth release of version *1.0*, newer than *1.0* or *1.0-1*,
	older than *1.0.1*.

*5:3.0-1*
	First release of version *3.0*, with epoch of *5*. Newer
	than for *6.0-1*, or *4:6.0-1*, older than *5:3.1-1*.

*1.0~beta1-3*
	Third release of 1.0-beta1 pre-version. Newer than *0.99* and
	*1.0~beta1-2*, older than *1.0*.

*2.0^20250611*
	First release of a version *2.0* based snapshot. Newer than
	*2.0*, older than *2.0.2*.

# BUGS
Various non-obvious behaviors and dark corners exist within the version
comparison algorithm, but are difficult to address due to high risk of
breaking existing packages:

- Non-ASCII characters are ignored and thus equal: *1.1.α* equals
  *1.1.β* and even *1.1.ββ*.
- Implicit segments can be deceptive: *1.f* is newer than *1c.f*. The
  result becomes more obvious by making the segments explicit:
  *1.f* is newer than *1.c.f* because the segments are compared one
  by one, and *c* sorts lower than *f* lexicographically.

*rpm*(8) *rpmbuild*(1) *rpmsort*(1)

*http://www.rpm.org/*
