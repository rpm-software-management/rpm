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

## Enable build conditionals

To enable a build conditional in a spec file, use the `%bcond` macro at the
beginning of the file, specifying the name of the conditional and its default
value:

```
# To build with "gnutls" by default:
%bcond gnutls 1
# To build without "gnutls" by default:
%bcond gnutls 0
```

The default can be any numeric expression.
To pass a complex expression as a single argument, you can enclose it in
`%[...]` .

```
# Add `--with openssl` and `--without openssl`, with the default being the
# inverse of the gnutls setting:
%bcond openssl %{without gnutls}

# Add `extra_tests` bcond, enabled by default if both of the other conditinals
# are enabled:
%bcond extra_tests %[%{with gnutls} && %{with sqlite}]
```


### Enabling using `%bcond_with` and `%bcond_without`

Build conditionals can also be enabled using the macros `%bcond_with` and
`%bcond_without`:

```
# add --with gnutls option, i.e. disable gnutls by default
%bcond_with gnutls
# add --without openssl option, i.e. enable openssl by default
%bcond_without openssl
```

If you want to change whether or not an option is enabled by default, only
change `%bcond_with` to `%bcond_without` or vice versa. In such a case, the
remainder of the spec file can be left unchanged.

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
