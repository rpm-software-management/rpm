---
layout: default
title: rpm.org - Users and Groups
---

# Users and Groups

Rpm >= 4.19 has native support for declarative user and group creation
through integration with systemd's
[sysusers.d](https://www.freedesktop.org/software/systemd/man/sysusers.d.html)
format.  Packagers will only need to package a sysusers.d file for
their custom users and groups in `/usr/lib/sysusers.d` and rpm will
take care of the rest.

It's also possible to declare sysusers.d entries manually with
`%add_sysuser` macro in the context of the (sub-)package. The macro
simply takes a sysusers.d line as arguments, such as
`%add_sysuser g mygroup 515`. Using this method is **not recommended** on
native systemd distributions.

Note that for certain types of systemd services (eg. transient and
socket activated services) it may be possible to avoid package level
user/group allocation altogether by using
[dynamic users](https://0pointer.net/blog/dynamic-users-with-systemd.html).

## Dependencies

Any non-root ownership in `%files` section (through `%attr()` or `%defattr()`)
generates an automatic dependency on the named user and/or group. Similarly,
packaged sysusers.d files create provides for the users and/or groups they
create. This ensures correct installation when a package relies
on user/group from another package.

Explict group membership (m) will also create a dependency on both the user
and the group name.

By default the dependencies are hard requirements, but as this can be
problematic when upgrading an existing distribution to rpm 4.19, it's possible
to weaken these into recommends-dependencies by setting 
`%_use_weak_usergroup_deps` macro to `1` during package builds.

## Limitations

At this time, rpm only supports the `u`, `g`, (since RPM 4.20) `m`
and (since RPM 6.0) the `u!` directives of sysusers.d format and
ignores others. If other directives are needed, the package will need
to call systemd-sysusers with the correct arguments manually.

## Technical details

As the sysusers.d data is needed before it's unpacked from the payload
during installation, rpm encodes the relevant parts into the EVR string
of generated user() and group() provides: the sysusers.d line is base64
encoded, with the input \0-padded as needed to avoid base64 '=' padding
in the output, which would be illegal in the EVR string. These are
decoded and fed into systemd-sysusers during installation, passing
`--replace <path>` and `--root <path>` as appropriate.

For example, the following sysusers.d line:

```
u dnsmasq - "Dnsmasq DHCP and DNS server" /var/lib/dnsmasq
```

...emits the following provides attached to the originating file:

```
user(dnsmasq) = dSBkbnNtYXNxIC0gIkRuc21hc3EgREhDUCBhbmQgRE5TIHNlcnZlciIgL3Zhci9saWIvZG5zbWFzcQAA
group(dnsmasq)
```

As systemd-sysusers implicitly creates a matching group for any created
users, the group provide does not have an EVR here, only explicitly
created groups will have the encoded sysusers.d line as EVR.

Lines adding users to groups like

```
m klangd klong
```

will create a provide:
```
groupmember(klangd/klong) = bSBrbGFuZ2Qga2xvbmcA
```
but also create two requires (or recommends if the `_use_weak_usergroup_deps` is set to 1):

```
group(klong)
user(klangd)
```

to make sure the packages creating the user and the group are install first.

# Implementation

RPM by default does not actually use `systemd-sysusers`, but has its
own shell script (`sysusers.sh`) that calls `useradd` and `groupadd`.
The program to call can be configured with the `%__systemd_sysusers`
macro. It is possible to point it to the `systemd-sysusers` utility,
but it may be undesired to add this as a dependency to RPM.
`systemd-sysusers` is normally installed as part of the main `systemd`
package, but in installations (for example containers) where `systemd`
is not needed, the smaller `systemd-standalone-sysusers` package which
does not pull in the rest of systemd may be used.

On systems that do neither have `useradd` and `groupadd` nor one of
the systemd packages, a custom script or program can be used. Such a
script needs to read sysusers.d lines from the standard input and then
call the native user and group creation tools as appropriate. The
script needs to handle `--root <path>` argument for chroot
installations - the script runs *from outside* of any possible chroot,
and care must be taken to avoid changing the host in such a case.
