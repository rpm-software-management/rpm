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
	ensure the integrity and origin of the package. See *VERIFICATION
	POLICY* for details of operation.

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

# VERIFICATION POLICY

To deal with three generations of package formats and as many decades of
cryptographic algorithms and differing system policies, the RPM package
verification is a complicated system with many moving parts.

RPM packages contain multiple individual _verifiables_ that cover different
parts of the package: header, payload or both. There are two _classes_ of
verifiables: digests and signatures. Each verifiable may use a different
cryptographic _algorithm_. A class-level verification result (pass or fail) is
computed based on the results from its individual verifiables. When a class or
verifiable passes, it is also said to _verify_.

For a class to verify, both the header and payload must be covered by a passing
verifiable. Non-trusted signatures (such as expired, or ones using legacy
algorithms) are ignored on the individual level, so they only cause a
class-level failure if no passing signature is present.

An outright failure of an individual verifiable typically fails the whole class
as well, but some verifiables have _alternatives_ where either of the
verifiables passing is sufficient and nullifies the other one failing.

The final verification result is computed based on the class results, and the
package verification mode set with the *%\_pkgverify\_level* macro (see
*rpm-config*(5) for details).

Which of the individual verifiables are used depends on the system cryptography
libraries and their policies, RPM configuration and the package format version.

# OUTPUT
*--checksig*
```
<_PACKAGE_FILE_>: [<digests|DIGESTS>] [<signatures|SIGNATURES>] <OK|NOT OK>
```

	The output is on the level of digest and signature class, which
	often includes results from multiple individual verifiables.
	Classes that verify are printed in lowercase, failing classes
	in UPPERCASE. The final package level conclusion per current
	*VERIFICATION POLICY* is printed out as *OK* or *NOT OK*.

*--list*
```
<fingerprint> <name> <userid> public key
```

# TROUBLESHOOTING

Use *--checksig --verbose* to troubleshoot verification on per-veriable level.

See *VERIFICATION POLICY* for background on interpreting the output.
The per-class results and the final conclusion are not printed at all in
this mode. One should not attempt to calculate the final result from
this output.

```
<_PACKAGE_FILE_>:
    <range> <description> [ALT] <class>: <result>
    ...
```

	The output is on the level of individual verifiables.

	Range describes the area which the verifiable covers. Possible
	values are:
	- *Header*: The main header of the package
	- *Payload*: The payload of the package
	- *Legacy*: The main header and the payload of the package

	Description includes general information about this verifiable.
	For digests, this is just the hash algorithm name, such as *SHA256*.
	Signatures are prefixed with *OpenPGP*.
	For discovered signatures, the rest of the description is one of the
	following, depending on whether the public key is available or not:

	```
	<version> <sign_algo>/<hash_algo>, key fingerprint <fprint>
	<version> <sign_algo>/<hash_algo>, key ID <id>
	```

	Note that the version is OpenPGP version, not to be confused with
	similar looking RPM signature versions.
	For non-present signatures such details are not available, but
	for RPM V3 and V4 signatures the "algorithm family" (*RSA* or *DSA*)
	is emitted. RPM V6 signatures may use arbitrary OpenPGP algorithms.

	*ALT* indicates the verifiable is an optional alternative to another
	verifiable.

	Class is one of *digest* or *signature*.

	Result is one of the following:
	- *OK*: Verification passed.
	- *NOKEY*: The public key is not available (signatures only).
	- *NOTTRUSTED*: Verifiable is not trusted, eg. expired key or a legacy
	  algorithm (signatures only). Does not necessarily indicate class
	  failure if there are other usable signatures present.
	- *NOTFOUND*: Verifiable was not found. Only emitted on class-level
	  failure, and indicates which individual verifiables RPM looked for.
	- *BAD*: Verification failed.


# CONFIGURATION

There are several configurables affecting the behavior of this
verification, see *rpm-config*(5) for details:
- *%\_keyring*
- *%\_keyringpath*
- *%\_pkgverify_flags*
- *%\_pkgverify_level*

# EXIT STATUS
On success, *0* is returned. In particular, for verification it means all
arguments were verified successfully as per current *VERIFICATION POLICY*.

A non-zero code indicates the number of failures encountered, but this
is capped to *254* to avoid overflowing the return. *255* is returned on
output error, this special code is used by *xargs*(1) to stop processing
immediately.

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
