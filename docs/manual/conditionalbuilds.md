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

## Enable `--with`/`--without` parameters

To use this feature in a spec file, add this to the beginning of the file:

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

To define `BuildRequires` depending on the command-line switch, you can use the
`%{with foo}` macro:

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

## Pass it to `%configure`

To pass options to configure or other scripts that understand a `--with-foo` or
`--without-foo` parameter, you can use the `%{?_with_foo}` macro:

```
%configure \
        %{?_with_gnutls} \
        %{?_with_openssl}
```

## References
* [macros](https://github.com/rpm-software-management/rpm/blob/master/macros.in)
