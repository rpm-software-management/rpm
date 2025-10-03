RPM-SETUP-AUTOSIGN(1)

# NAME
*rpm-setup-autosign* - Set up automatic signing for rpmbuild

# SYNOPSIS
*rpm-setup-autosign* [options]

# DESCRIPTION
*rpm-setup-autosign* is used to set up automatic signing from *rpmbuild*(1).
It generates a user- and host-specific, passwordless OpenPGP key,
configures *rpmbuild*(1) to use that key and exports the public key
(aka certificate) for importing to the persistent *rpm*(8) keyring.

The purpose of automatic signing is to make testing local builds
painless. For distributing packages, it's recommended to use a separate
signing account that cannot be compromised by a build.

# OPTIONS
*-p* <*gpg*|*sq*>,
*--prog* <*gpg*|*sq*>
	Specify the signing program to use: GnuPG or Sequoia PGP's sq.

# CONFIGURATION
On successful completion, one or more of the following macros will be
configured in the user's macro configuration file.
See *rpm-config*(5) for details:
- *%\_openpgp_autosign_id*
- *%\_openpgp_sign*

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# FILES
_~/.config/rpm/rpmbuild-\*.asc_++
_~/.config/rpm/macros_

# EXAMPLES
*/usr/lib/rpm/rpm-setup-autosign -p sq*
	Set up *rpmbuild*(1) autosigning using Sequoia-sq.

# SEE ALSO
*rpm*(8) *rpmsign*(1) *rpmbuild*(1)

*http://www.rpm.org/*
