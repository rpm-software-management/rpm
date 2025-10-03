RPMKEYS(8)

# NAME

rpmkeys - RPM Keyring

# SYNOPSIS

*rpmkeys* {*-K*|*--checksig*} [options] _PACKAGE_FILE_ ...

*rpmkeys* {*-d*|*--delete*|*-e*|*--erase*} [options] _FINGERPRINT_ ...

*rpmkeys* {*-x*|*--export*} [options] [_FINGERPRINT_ ...]

*rpmkeys* {*-i*|*--import*} [options] _PUBKEY_ ...

*rpmkeys* {*-l*|*--list*} [options] [_FINGERPRINT_ ...]

*rpmkeys* *--rebuild* [options] [rebuild-options]

# DESCRIPTION

*rpmkeys* is used for manipulating the *rpm* keyring and verifying
package digital signatures with the contained keys.

For all available operations, see *OPERATIONS*.

# OPERATIONS

*-K*,
*--checksig*
	Verify the digests and signatures contained in _PACKAGE_FILE_ to
	ensure the integrity and origin of the package.

*-d*,
*--delete*,
*-e*,
*--erase*
	Erase the key(s) designated by _FINGERPRINT_.
	The *--delete* and *-d* options are deprecated.

*-x*,
*--export*
	Output the key(s) designated by _FINGERPRINT_ using an ASCII-armor
	encoding.  If _FINGERPRINT_ is not specified, output all keys.

*--import*
	Import ASCII-armored public keys.
	Digital signatures cannot be verified without the corresponding
	public key (aka certificate).

*-l*,
*--list*
	List currently imported public key(s) (aka certificates) by their
	fingerprint and user ID. If no fingerprints are specified, list all
	keys.

*--rebuild*
	Recreate the public key storage. Update to the latest format and drop
	unreadable keys.

# ARGUMENTS

_FINGERPRINT_
	The handle used for all operations on the keys.
_PACKAGE_FILE_
	An *rpm* package file or a manifest.
_PUBKEY_
	An ASCII-armored OpenPGP public key (aka certificate).

# OPTIONS

See *rpm-common*(8) for the options common to all *rpm* executables.

# REBUILD OPTIONS

*--from* <*fs*|*openpgp*|*rpmdb*>
	Use the keys from the specified backend to rebuild the currently
	configured keystore backend.
	This can be used to convert from one key storage to another.

# OUTPUT
*--checksig*
```
<_PACKAGE_FILE_>: <element> <element> <OK|NOT OK>
```

	With *--verbose*:
```
<_PACKAGE_FILE_>:
    <element>: <OK|NOTFOUND|BAD>
    ...
```

*--list*
```
<fingerprint> <name> <userid> public key
```

# CONFIGURATION

There are several configurables affecting the behavior of this
verification, see *rpm-config*(5) for details:
- *%\_keyring*
- *%\_keyringpath*
- *%\_pkgverify_flags*
- *%\_pkgverify_level*

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES

*rpmkeys --export 771b18d3d7baa28734333c424344591e1964c5fc | sq inspect*
	Export key 771b18d3d7baa28734333c424344591e1964c5fc for inspecting
	with sequoia-sq.

*rpmkeys --erase 771b18d3d7baa28734333c424344591e1964c5fc*
	Erase key 771b18d3d7baa28734333c424344591e1964c5fc from the keyring.

*rpmkeys -K hello-2.0-1.x86_64.rpm*
	Verify hello-2.0-1.x86_64.rpm package file.

# SEE ALSO
*popt*(3), *rpm*(8), *rpm-common*(8), *rpm-config*(5), *rpmsign*(1)

*rpmkeys --help* - as *rpm*(8) supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
