RPMKEYS(8)

# NAME

rpmkeys - RPM Keyring

# SYNOPSIS

*rpmkeys* {*--list*|*--import*|*--erase*|*--delete*|*--checksig*}

# DESCRIPTION

The general forms of *rpm*(8) digital signature commands are

*rpmkeys* {*-l*|*--list*} [_FINGERPRINT_ ...]

*rpmkeys* {*-x*|*--export*} [_FINGERPRINT_ ...]

*rpmkeys* {*-i*|*--import*} _PUBKEY_ ...

*rpmkeys* {*-e*|*--erase*|*-d*|*--delete*} _FINGERPRINT_ ...

*rpmkeys* {*-K*|*--checksig*} _PACKAGE_FILE_ ...

The *--checksig* option checks all the digests and signatures
contained in _PACKAGE_FILE_ to ensure the integrity and origin of the
package. Note that signatures are now verified whenever a package is
read, and *--checksig* is useful to verify all of the digests and
signatures associated with a package.

Digital signatures cannot be verified without a public key. An ASCII
armored public key can be added to the *rpm* persistent keyring using
*--import*.

The following commands are available for manipulating the persistent
rpm keyring:

*rpmkeys* {*-l*|*--list*} [_FINGERPRINT_...]

List currently imported public key(s) (aka certificates) by their
fingerprint and user ID. If no fingerprints are specified, list all keys.

The fingerprint is the handle used for all operations on the keys.

*rpmkeys* {*-x*|*--export*} [_FINGERPRINT_ ...]

Output the key(s) using an ASCII-armor encoding.

Exporting allows for inspecting the data with specialized tools, such
as Sequoia or GnuPG. For example:

*rpmkeys --export 771b18d3d7baa28734333c424344591e1964c5fc | sq inspect *

*rpmkeys* {*-e*|*--erase*|*-d*|*--delete*} _FINGERPRINT_ ...

Erase the key(s) designated by _FINGERPRINT_. For example:

*rpmkeys* *--erase 771b18d3d7baa28734333c424344591e1964c5fc*

# SEE ALSO

*popt*(3), *rpm*(8), *rpmdb*(8), *rpmsign*(1), *rpm2cpio*(1),
*rpmbuild*(1), *rpmspec*(1)

*rpmkeys --help* - as rpm supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
