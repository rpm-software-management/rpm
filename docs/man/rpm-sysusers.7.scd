RPM-SYSUSERS(7)

# NAME
*rpm-sysusers* - RPM native allocation of system users and groups

# SYNOPSIS

## Declarations
*%{\_sysusersdir}*_/\*.conf_++
*%{add_sysuser *_LINE_*}*

Note: _LINE_ denotes the sysusers.d line fields passed as separate arguments

## Provides
*user(*_UNAME_*)* = _BASE64_++
*group(*_GNAME_*)* = [_BASE64_]++
*groupmember(*_UNAME_*/*_GNAME_*)*

Note: _BASE64_ denotes the corresponding sysusers.d line encoded with base64

## Requires
*user(*_UNAME_*)*++
*group(*_GNAME_*)*

## Tunables
*%\_sysusersdir*++
*%\_\_systemd_sysusers*++
*%\_use_weak_usergroup_deps*

# DESCRIPTION

RPM has native support for the *sysusers.d*(5) format (introduced by systemd)
that enables packages to declare users and/or groups to be automatically created
at installation time. This eliminates the need for the manual creation of such
accounts in transaction scriptlets.

This works implicitly if a package ships one or more files in this format under
*%\_sysusersdir* (typically _/usr/lib/sysusers.d/_). RPM will then automatically
invoke the *%\_\_systemd_sysusers* binary (typically *systemd-sysusers*(8)) with
the correct arguments in order to create the user and/or group accounts when the
package is installed. This is the recommended method on systemd native platforms
and requires no intervention from the package beyond shipping those files.

RPM also allows for declaring sysusers.d entries explicitly in the spec file
with the *%add_sysuser* macro. This macro takes a sysusers.d line with its
fields passed as separate arguments and must be declared in the (sub-)package
context. This method is primarily intended for platforms without systemd where
it is typically also coupled with a custom *%\_\_systemd_sysusers* binary (see
*DECODING* for details).

Note that for certain types of systemd services (for example, transient or
socket activated), it may be possible to avoid package level user and group
allocation altogether by using Dynamic Users (see *DynamicUser=* in
*systemd.exec*(5) and the URL in *SEE ALSO* for details).

Added: 4.19.0

# DEPENDENCIES

RPM will generate an automatic requires on the named user or group for every
non-root ownership declared through the *%attr()* or *%defattr()* directive in
the *rpm-spec*(5) file. Similarly, it will generate a provides for every user
and group declared in the packaged sysusers.d files and/or standalone entries.
This ensures correct installation when a package relies on a user and/or group
from another package.

Explicit group membership (the *m* sysusers.d directive) will also create a
dependency on both the user and the group name.

By default, the dependencies are hard requires. This can be problematic when
upgrading an existing distribution to RPM 4.19, however, so it is possible to
weaken these requires into recommends, by setting *%\_use_weak_usergroup_deps*
to *1* in the *rpmbuild-config*(5).

# LIMITATIONS

Currently, RPM only supports the following sysusers.d directives:
- *u* and *g* (Added: 4.19.0)
- *m* (Added: 4.20.0)
- *u!* (Added: 4.20.1)

Other directives are ignored. If a package needs such unsupported directives, it
should call *systemd-sysusers*(8) (or its equivalent) with the correct arguments
manually.

# ENCODING

Each generated *user(*_UNAME_*)*, *group(*_GNAME_*)* and
*groupmember(*_UNAME_*/*_GNAME_*)* provides, where _UNAME_ and _GNAME_ are the
respective user and group names, also contains the original sysusers.d line in
its EVR string. The sysusers.d line is base64 encoded (as per RFC 4648), with
the input *\\0*-padded as needed, in order to avoid the base64 native *=*
padding in the output, which is illegal in an EVR string. This arrangement
allows RPM to obtain the sysusers.d data before unpacking the payload to disk.

For example, the following sysusers.d line:

```
u dnsmasq - "Dnsmasq DHCP and DNS server" /var/lib/dnsmasq
```

would emit the following provides attached to the originating file:

```
user(dnsmasq) = dSBkbnNtYXNxIC0gIkRuc21hc3EgREhDUCBhbmQgRE5TIHNlcnZlciIgL3Zhci9saWIvZG5zbWFzcQAA
group(dnsmasq)
```

As *systemd-sysusers*(8) implicitly creates a matching group for any created
users, the group provides does not have an EVR here. Only explicitly created
groups will have the encoded sysusers.d line as an EVR.

Lines adding users to groups, such as:

```
m klangd klong
```

will create a provides:

```
groupmember(klangd/klong) = bSBrbGFuZ2Qga2xvbmcA
```

They will also create two requires (or recommends if
*%\_use_weak_usergroup_deps* is set to *1*):

```
group(klong)
user(klangd)
```

to make sure the packages creating the user and group are installed first.

# DECODING

Each *user()*, *group()*, and *groupmember()* EVR is decoded during installation
and fed into the program specified by the *%\_\_systemd_sysusers* macro that
creates the requested accounts on the system.

This program must interpret each line on standard input and create users and/or
groups accordingly, preferably by calling the account creation tools native to
the platform. The program must also handle the *--root* argument for *chroot*(1)
installations, since it is executed _from outside_ of any possible chroot and
care must be taken to avoid changing the host in such a case.

RPM itself ships with a simple shell script at *%{\_rpmconfigdir}*_/sysusers.sh_
that implements this interface. This script calls the standard *useradd*(8) and
*groupadd*(8) programs and is the stock *%\_\_systemd_sysusers* configuration.
It is intended for use on platforms without systemd, or where a full dependency
on systemd (such as in containers) is not desired and a smaller package that
only provides the original binary (like *systemd-standalone-sysusers* on Fedora
Linux) is not available.

Note that the packaged sysusers.d files, if any, are installed as normal but do
not participate in the user and/or group creation itself. Instead, they simply
serve as persistent configuration files as per *sysusers.d*(5).

# EXAMPLES
## Example 1. Packaging a sysusers.d file (implicit method)

Consider a _dnsmasq.spec_ file with the following contents (irrelevant parts
ellipsized or omitted):

```
%install
mkdir -p ${RPM_BUILD_ROOT}/%{_sysusersdir}
cat << EOF > ${RPM_BUILD_ROOT}/%{_sysusersdir}/dnsmasq.conf
u dnsmasq - "Dnsmasq DHCP and DNS server" /var/lib/dnsmasq
EOF
[...]

%files
%dir %attr(0755,root,dnsmasq) %{_var}/lib/dnsmasq
%{_sysusersdir}/dnsmasq.conf
[...]
```

When a package is built, it will contain the following provides:

```
$ rpm -qp --provides /path/to/dnsmasq.rpm
[...]
group(dnsmasq)
user(dnsmasq) = dSBkbnNtYXNxIC0gIkRuc21hc3EgREhDUCBhbmQgRE5TIHNlcnZlciIgL3Zhci9saWIvZG5zbWFzcQAA
```

Finally, when the package is installed, RPM will create the *dnsmasq* user and
group automatically, and the _/var/lib/dnsmasq_ directory installed by the
package itself will be owned by that group as per the spec file:

```
$ getent passwd dnsmasq
dnsmasq:x:999:999:Dnsmasq DHCP and DNS server:/var/lib/dnsmasq:/usr/sbin/nologin
$ getent group dnsmasq
dnsmasq:x:999:
$ stat -c '%G:%g' /var/lib/dnsmasq
dnsmasq:999
```

The original sysusers.d file will be installed as normal:

```
$ rpm -ql dnsmasq | grep sysusers
/usr/lib/sysusers.d/dnsmasq.conf
$ cat /usr/lib/sysusers.d/dnsmasq.conf
u dnsmasq - "Dnsmasq DHCP and DNS server" /var/lib/dnsmasq
```

## Example 2. Declaring a sysusers.d line (explicit method)

Consider a _dnsmasq.spec_ file with the following contents starting just below
the preamble block (irrelevant parts ellipsized or omitted):

```
%{add_sysuser u dnsmasq - "Dnsmasq DHCP and DNS server" /var/lib/dnsmasq}
[...]

%files
%dir %attr(0755,root,dnsmasq) %{_var}/lib/dnsmasq
[...]
```

Building and installing this package will have the same effect as described in
*Example 1*. The only difference is that, in this example, no sysusers.d file is
packaged or installed.

# SEE ALSO

*rpm*(8) *rpmbuild-config*(5) *rpm-scriptlets*(7) *rpm-spec*(5) *sysusers.d*(5)
*systemd-sysusers*(8) *systemd.exec*(5) *useradd*(8) *groupadd*(8)

*https://0pointer.net/blog/dynamic-users-with-systemd.html*
