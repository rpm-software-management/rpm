---
layout: default
title: rpm.org - Conditional Builds
---
# Conditional Builds

Rpmbuild supports conditional package builds with the command line switches
`--with` and `--without`.

Here is an example of how to enable gnutls and disable openssl support:

```
$ rpmbuild -ba newpackage.spec --with gnutls --without openssl
```

## Creating build conditionals

### Using `%bcond` (new in rpm 4.17.1)

To create a build conditional in a spec file, use the `%bcond` macro at the
beginning of the file, specifying the name of the conditional and its default
value:

```
# Create a "gnutls" build conditional, enabled by default:
%bcond gnutls 1
# Create a "bootstrap" build conditional, disabled by default:
%bcond bootstrap 0
```

The default can be any numeric expression.
To pass a complex expression as a single argument, you can enclose it in
`%[...]` .

```
# Create `--with openssl` and `--without openssl` cli options, with the default
# being the inverse of the gnutls setting:
%bcond openssl %{without gnutls}

# Create `extra_tests` bcond, enabled by default if both of the other
# conditinals # are enabled:
%bcond extra_tests %[%{with gnutls} && %{with sqlite}]
```

### Using `%bcond_with` and `%bcond_without`

Build conditionals can also be created using the macros `%bcond_with` and
`%bcond_without`. These macros *create command line options*. When you add
an option to build with feature X, it implies that the default is to build
without.

These macros have historically confused a lot of people (which is why
`%bcond` was added) but are not hard to use once you think in terms
of adding command line switches:

```
# Create an option to build with gnutls (`--with gnutls`), thus default to
# building without it
%bcond_with gnutls
# Create an option to build without openssl (`--without openssl`), thus default
# building with it
%bcond_without openssl
```

To change the default, change the command line switch from `with` to `without`
or vice versa. The remainder of the spec file can be left unchanged.

## Check whether an option is enabled or disabled

To make parts of the spec file conditional depending on the command-line
switch, you can use the `%{with foo}` macro or its counterpart,
`%{without foo}`:

```
%if %{with gnutls}
BuildRequires:  gnutls-devel
%endif
%if %{with openssl}
BuildRequires:  openssl-devel
%endif
```

Alternatively, you can test the presence (or lack thereof) of `%with_foo`
macros which is nicer in other situations, e.g.:

```
%configure \
   %{?with_static:--enable-static} \
   %{!?with_static:--disable-static}
```

Always test for the `with`-condition, not the `without`-counterpart!

## References
* [macros](https://github.com/rpm-software-management/rpm/blob/master/macros.in)
