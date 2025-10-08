RPMSIGN(1)

# NAME
rpmsign - RPM Package Signing

# SYNOPSIS
*rpmsign* {*--addsign*|*--resign*} [options] [sign-options] _PACKAGE_FILE_ ...

*rpmsign* *--delsign* [options] _PACKAGE_FILE_ ...

*rpmsign* *--delfilesign* [options] _PACKAGE_FILE_ ...

# DESCRIPTION
*rpmsign* is used to manipulate digital OpenPGP signatures on *rpm* package
files.

To create a signature rpmsign needs to verify the package's checksum. As a
result V4 packages with MD5/SHA1 checksums cannot be signed in FIPS mode.

# OPERATIONS
*--addsign*
	Generate and insert a new OpenPGP signature
	for each _PACKAGE_FILE_ given unless a signature with identical
	parameters already exists, in which case no action is taken.
	Arbitrary number of V6 signatures can be added.

*--resign*
	Generates and inserts a new OpenPGP signature
	for each _PACKAGE_FILE_, replacing any and all previous signatures.

*--delsign*
	Delete all OpenPGP signatures from each package _PACKAGE_FILE_ given.

*--delfilesign*
	Delete all IMA and fsverity file signatures from each package
	_PACKAGE_FILE_ given.

# ARGUMENTS
_PACKAGE_FILE_
	An *rpm* package file.

# OPTIONS
See *rpm-common*(8) for the options common to all *rpm* executables.

# SIGN OPTIONS
*--certpath* _CERT_
	Used with *--signverity*, use file signing certificate _CERT_.

*--fskpath* _KEY_
	Used with *--signfiles*, use file signing key _KEY_.

*--key-id* _KEYID_
	Use key _KEYID_ for signing. Overrides *%\_openpgp_sign_id*
	configuration.

*--rpmv3*
	Request RPM V3 header+payload signature addition on V4 packages.
	These signatures are expensive
	and redundant baggage on packages where a separate payload digest
	exists (packages built with rpm >= 4.14). Rpmsign will automatically
	detect the need for V3 signatures, but this option can be used to
	request their creation if the packages must be fully signature
	verifiable with rpm < 4.14 or other interoperability reasons.

	Has no effect when signing V6 packages.

*--rpmv4*
	Request RPM V4 header signature addition on V6 packages.
	Useful for making V6 packages signature verifiable
	with rpm 4.x versions.

	V4 compatibility signatures are only ever added if the signing
	algorithm is one of those known to V4: RSA, EcDSA, EdDSA
	(and original DSA).
	Only one V4 signature can be present in a package, so this is
	added only on the first *--addsign* with a V4 compatible
	algorithm, and ignored otherwise.

	Has no effect when signing V4 packages.

*--rpmv6*
	Request RPM V6 header signature addition on V4 packages.

	This generally always succeeds as there can be arbitrary number of
	V6 signatures on a package. A V3/V4 compatibility signatures are
	added using the same logic as *--rpmv4* on a V6 package.

	Has no effect when signing V6 packages.

*--signfiles*
	Sign package files. The file signing key (RSA private key) must
	be set before signing the package, it can be configured on the
	command line with *--fskpath* or the macro %\_file_signing_key.

*--signverity*
	Sign package files with fsverity signatures. The file signing key
	(RSA private key) and the signing certificate must be set before
	signing the package. The key can be configured on the command line
	with *--fskpath* or the macro %\_file_signing_key, and the cert
	can be configured on the command line with *--certpath* or the
	macro %\_file_signing_cert.

*--verityalgo* _ALG_
	Used with *--signverity*, to specify the signing algorithm.
	sha256 and sha512 are supported, with sha256 being the default if
	this argument is not specified. This can also be specified with the
	macro *%\_verity_algorithm*.

# CONFIGURATION
In order to sign packages, you need to create your own OpenPGP key pair
(aka certificate) and configure *rpm*(8) to use it. The following macros are
available:

*%\_openpgp_sign_id*
	The fingerprint or keyid of the signing key to use. Typically
	this is the only configuration needed. If omitted,
	*--key-id* must be explicitly specified when signing.

*%\_openpgp_sign*
	The OpenPGP implementation to use for signing. Supported values are
	"gpg" for GnuPG (default and traditional) and "sq" for Sequoia PGP.

Implementation specific macros:

*%\_gpg_path*
	The location of your GnuPG keyring if not the default *$GNUPGHOME*.

*%\_gpg_name*
	Legacy macro for configuring user id with GnuPG. Use the implementation
	independent and non-ambiguous *%\_openpgp_sign_id* instead.

*%\_sq_path*
	The location of your Sequoia configuration if not the default.

# EXAMPLES
## Example 1. Basic setup
Configure RPM to sign packages with Sequoia PGP and a specific key by adding the
following contents to the user's *rpm-config*(5) file (typically
_~/.config/rpm/macros_):

```
%\_openpgp_sign sq
%\_openpgp_sign_id 7B36C3EE0CCE86EDBC3EFF2685B274E29F798E08
```

## Example 2. Basic operations
*rpmsign --addsign hello-2.0-1.x64_rpm*
	Add a signature to _hello-2.0-1.x64_rpm_ package.

*rpmsign --resign --key-id 771b18d3d7baa28734333c424344591e1964c5fc hello-2.0-1.x64_rpm*
	Replace all signatures in _hello-2.0-1.x64_rpm_ package by
	a signature using key *771b18d3d7baa28734333c424344591e1964c5fc*.

*rpmsign --delsign --delfilesign hello-2.0-1.x64_rpm*
	Delete all signatures from _hello-2.0-1.x64_rpm_ package.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# SEE ALSO
*popt*(3), *rpm*(8), *rpm-common*(8), *rpmkeys*(8), *rpmbuild*(1)

*rpmsign --help* - as rpm supports customizing the options via popt
aliases it's impossible to guarantee that what's described in the
manual matches what's available.

*http://www.rpm.org/*
