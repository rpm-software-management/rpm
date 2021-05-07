---
layout: default
title: rpm.org - Immutable header regions
---
# Immutable header regions in rpm-4.0.1 and later

The header data structure has changed in rpm-4.0.[12] to preserve the
original header from a package. The goal is to keep the original
header intact so that metadata can be verified separately from the
payload by the RHN up2date client and by the rpm command line verify
mode using signatures saved in the rpm database. I believe the change
is entirely forward and backward compatible, and will not require
any artifacts like changing the version number of packaging or 
adding an "rpmlib(...)" tracking dependency. We'll see ...

Here's a short description of the change. An rpm header has three sections:
```
	1) intro		(# entries in index, # bytes of data)
	2) index		16 byte entries, one per tag, big endian
	3) data			tag values, properly aligned, big endian
```

Representing sections in the header (ignoring the intro) with
```
	A,B,C			index entries sorted by tag number
	a,b,c			variable length entry data
	| 			boundary between index/data
```
a header with 3 tag/value pairs (A,a) can be represented something like
```
	ABC|abc
```

The change is to introduce a new tag that keeps track of a contiguous
region (i.e. the original header). Representing the boundaries with
square/angle brackets, an "immutable region" in the header thus becomes
```
	[ABC|abc]
```
or more generally (spaces added for clarity)
```
	[ABC> QRS | <abc] qrs
```
or with concatenated regions (not implemented yet)
```
	[ABC> [DEF> QRS | <abc] <def] qrs
```
or with nested regions (not implemented yet)
```
	[ABC [DEF>> QRS | <<abc] def] qrs
```

\todo Either concatenated/nested regions may be used to implement
a metarpm, aka a package of packages, dunno how, when, or even if, yet.

What complicates the above is legacy issues, as various versions of rpm
have added/deleted/modified entries in the header freely. Thus, representing
altered tag entries/data with a '.', there is a need to preserve deleted
information something like

```
	[A.C> QRS XYZ | <a.c] qrs xyz
```

\note This is basically the change that replaces the filename with
	a {dirname,basename,dirindex} triple between rpm-3.x and rpm-4.x.

or

```
	[AB.> QRS D | <ab.] qrs d
```

\note The header is no longer sorted because of replacing Cc with Dd.

and yet permit retrieval of the original

```
	[ABC|abc]
```

region. PITA, really.

What made header regions trickier yet is the desire to have an implementation
that is both backward and forward compatible. I won't bore you with the
tedious details.

However, even after doing regressions against supported Red Hat releases,
there's a Great Big Universe of rpm packages out there, and I'm *very*
interested in hearing (as bug reports against rpm at http://bugzilla.redhat.com)
about any and all problems with header regions in rpm-4.0.1.
