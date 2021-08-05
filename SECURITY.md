# Security vulnerability reporting

Security vulnerabilities should be reported via email to
[Red Hat Product Security](mailto:secalert@redhat.com).  Please PGP-encrypt the
email if possible.

Examples of security vulnerabilities include, but are not limited to:

- Memory corruption when parsing or querying a malicious package.
- Bypassing signature validation as performed by `rpmkeys -K` command, or via
  the `pgpVerifySignature()`, `rpmVerifySignatures()`, or
  `rpmcliVerifySignatures()` API functions.
- Reporting that a signature is present when a package is queried, and then
  failing to reject the package when attempting to install it, even if the
  signature is invalid or was not made by a trusted key.
- Being able to modify an RPM package such that it passes signature verification,
  but installing it corrupts the RPM database.
- Successfully installing an unsigned, or improperly signed, package if
  `%_pkgverify_level` is not set to `none` or `digest`.
- Successfully installing an improperly signed package, if `%_pkgverify_level`
  is not set to `none` and `%_pkgverify_flags` is zero.

Generally, undefined pointer arithmetic is not considered a security
vulnerability on its own, unless a compiler compiles the code in a way that
causes a security problem in the generated code.
