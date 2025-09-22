RPM-VERSION(7)

# NAME
*rpm-version* - RPM version system

# SYNOPSIS
\[_EPOCH_*:*]_VERSION_[*-*_RELEASE_]

# DESCRIPTION
A label known as _EVR_ is used to refer to software versions in RPM,
consisting of up to three _components_:

- _VERSION_ reflects the actual packaged software version.
- _RELEASE_ reflects packaging revisions within that software version.
- _EPOCH_ is an artificial override to allow working around versioning
  anomalies.

Only the _VERSION_ component is mandatory in an _EVR_ label.
All RPM packages have a _VERSION_ and a _RELEASE_, however.

## Version
The _VERSION_ string reflects the actual packaged software version.

The string consists of ASCII alphanumeric characters, optionally
_segmented_ with the _separators_ period (*.*), underscore (*\_*) and
the plus sign (*+*), and _operators_ tilde (*~*) and caret (*^*).

The _operators_ are used to map pre- and post-release version strings
into a coherent upgrade.

Notably, the dash character (*-*) can NOT be used in _VERSION_ or _RELEASE_
as it is the component separator.

## Release
The _RELEASE_ reflects a revision within a single software version.
Ideally, _RELEASE_ is a simple integer that is incremented whenever changes
are made to a package, and should be reset (to 1) whenever the software
version changes.

Technically, the format is exactly the same as for _VERSION_.

## Epoch
The _EPOCH_ is a non-negative integer, separated from the version with
a colon (*:*). It's the most significant part of an
_EVR_, skewing version comparison to make an older version to appear newer.
It's sometimes necessary to work around version anomalies such as a software
project changing its versioning scheme, but also packaging errors.

An omitted _EPOCH _ has an implicit value of zero.

The epoch should be only used as a last resort. It violates the principle
of least surprise, and changing it requires all related versioned
dependencies in other packages to be updated accordingly.

## Comparing
Two _EVR_\s are compared left to right, one component at a time.
The components are compared left to right, a _segment_ at a time.
The comparison stops as soon as a segment or component level difference
is found, so if eg. the epochs differ, the rest of the _EVR_ is not
considered at all.

Within components, consecutive alphabetic characters and consecutive numbers
form _implicit segments_. _Explicit segments_ are denoted by _separators_
and _operators_.
Numeric segments are compared numerically as integers with leading zeros
ignored, otherwise lexicographical comparison is used.
That is, *abc123* consists of two segments: *abc* and *123* and is equal
to *abc0123*, *abc.123* and *abc.000123* despite difference in appearance.

Numeric segments are considered newer than alphabetic segments regardless
of the actual content. When otherwise equal, the component with more
segments is considered newer, and similarly an _EVR_ with more components
is considered newer. For example, *0.0* is newer than *0* and *1.xyz* is
older than *1.0* but newer than *1*.

The segment _separator_ characters are not compared, so they can be used
interchangeably, and multiple consecutive separators are treated as if
only one separator was used. Thus, *1.0* is equal to *1+0* and *1+.+0*.

The tilde operator causes a segment to sort older, and is used for
_pre-release versions_. For example *2.0~beta1* is older than *2.0*
or *2.0~rc1*, and newer than *1.0*.
The caret operator is essentially the opposite of tilde. It causes
a segment to sort newer, and is used for _post-release snapshots_
that exist between actual software releases.
For example, *2.0^150825* is newer than *2.0* but older than *2.0.1*.

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

*1.0~beta2*
	Pre-release *beta2* of version *1.0*. Newer than *0.99* and *1.0~beta1*,
	older than *1.0*.

*2.0^20250611*
	Post-release snapshot *20250611* of version *2.0*. Newer than *2.0*,
	older than *2.0.1*.

# BUGS
Various non-obvious behaviors and dark corners exist within the version
comparison algorithm, but are difficult to address due to high risk of
breaking existing packages:

- Non-ASCII characters are ignored and thus equal: *1.1.α* equals
  *1.1.β* and even *1.1.ββ*. *rpmbuild*(1) rejects such EVR\s but it's
  possible to encounter "illegal" values via API usage, very old packages
  and packages created with 3rd party tools.
- Implicit segments can be deceptive: *1.f* is newer than *1c.f*. The
  result becomes more obvious by making the segments explicit:
  *1.f* is newer than *1.c.f* because the segments are compared one
  by one, and *c* sorts lower than *f* lexicographically.

*rpm*(8) *rpmbuild*(1) *rpmsort*(1)

*http://www.rpm.org/*
