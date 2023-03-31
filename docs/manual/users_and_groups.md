---
layout: default
title: rpm.org - Users and Groups
---

# Users and Groups

Rpm >= 4.19 has native support for declarative user and group creation
through integration with systemd's
[sysusers.d](https://www.freedesktop.org/software/systemd/man/sysusers.d.html).
Packagers will only need to package a sysusers.d file for their custom users
and groups in `/usr/lib/sysusers.d` and rpm will take care of the rest.

It's also possible to declare sysusers.d entries manually with
`%add_sysuser` macro in the context of the (sub-)package. The macro
simply takes a sysusers.d line as arguments, such as
`%add_sysuser g mygroup 515`. Using this method is **not recommended** on
native systemd distributions.

## Dependencies

Any non-root ownership in `%files` section (through `%attr()` or `%defattr()`)
generates an automatic dependency on the named user and/or group. Similarly,
packaged sysusers.d files create provides for the users and/or groups they
create. This ensures correct installation when a package relies
on user/group from another package.

By default the dependencies are hard requirements, but as this can be
problematic when upgrading an existing distribution to rpm 4.19, it's possible
to weaken these into recommends-dependencies by setting 
`%_use_weak_usergroup_deps` macro to `1` during package builds.

## Limitations

At this time, rpm only supports the `u` and `g` directives of sysusers.d
format and ignores others. If other directives are needed, the package
will need to call systemd-sysusers with the correct arguments manually.

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

# Non-systemd operating systems

On non-systemd platforms, it's possible to use either
systemd-standalone-sysusers as a drop-in solution, or rpm can be configured
to use a custom script instead of systemd-sysusers for user and group
creation by overriding `%__systemd_sysusers` macro in the main rpm
configuration.

Such a script needs to read sysusers.d lines from the standard input
and interpret these into calls native user and group creation tools as
appropriate. The script needs to handle `--root <path>` argument for
chroot installations - the script runs *from outside* of any possible chroot,
and care must be taken to avoid changing the host in such a case.

