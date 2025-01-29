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
%{!?with_bootstrap:BuildRequires: ...}
```

or

```
%configure \
   %{?with_static:--enable-static} \
   %{!?with_static:--disable-static}
```

Always test for the `with`-condition, not the `without`-counterpart!

## Overriding Defaults

For distributions it can be useful to override the build conditionals on a global scale. To not interfere with the users ability to overwrite the conditionals on the command line there is an option to override the default value independently of the one chosen in the spec file.

To do this one can define a `%bcond_override_default_NAME` macro as one or zero or use the `%{bcond_override_default NAME VALUE}` macro. Distributions can put the former into a global macro file that is installed during local builds to propagate these changed defaults outside their build system. Using different versions of the macro file allows building the same set of packages in different ways - e.g. against different libraries - without altering all the spec files.

E.g. add this in the macros file to disable support for zstd assuming this is a common conditional in the distribution:
```
%bcond_override_default_zstd 0
```

All packages with a `zstd` bcond will now build as if the bcond was defined as `%bcond zstd 0`.
I.e. unless `--with zstd` is used, the bcond will be disabled.

## References
* [macros](https://github.com/rpm-software-management/rpm/blob/master/macros.in)
