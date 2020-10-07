# Passing conditional parameters into a rpm build

Source code is often built with optional features enabled or disabled.
When source code is packaged using rpm, the various features can be
chosen, added to a spec file, and a package will be produced with
binaries compiled with that feature set. This mechanism works fine
for packages with small feature sets, but does not work so well
for large, complicated, packages like the Linux kernel and/or
the Pine mailer which have a large number of features, as a given
feature set may not "work" for everyone.

RPM now has a supported mechanism to pass information from the rpm
command line to enable/disable features during a build. Two options have
been added to pass feature names from the rpm command line:
```
    --with <feature>	Enable <feature>
    --without <feature>	Disable <feature>
```
The new options are implemented using popt to add aliases to the existing rpm
options --define to specify macros from the command line. The magic necessary
to add the new options is (from the file /usr/lib/rpm/rpmpopt*)
```
    rpmb	alias --with    --define "_with_!#:+       --with-!#:+"
    rpmb	alias --without --define "_without_!#:+    --without-!#:+"
```
(Note: The obscure "!#:+" popt token above says "substitute the next command
line argument found here, and, additionally, mark the argument as used.")

For example, when rpm is invoked as
```
    rpm ... --with ldap ...
```
then the popt aliases will cause the options to be rewritten as
```
    rpm ... --define "_with_ldap	--with-ldap" ...
```
which causes a "%_with_ldap" macro to be defined with value "--with-ldap"
during a build.

The macro defined on the rpm command line can be used to conditionalize
portions of the spec file for the package. For example, let's say you
are trying to build the pine package using "--with ldap" to enable the
LDAP support in the pine mailer (i.e. configuring with "--with-ldap").
So the spec file should be written
```
    ...
    ./configure \
	%{?_with_ldap}   \
    ...
```
so that, if "--with ldap" was used as a build option, then configure
will be invoked (after macro expansion) as
```
	./configure --with-ldap ...
```
(Note: The obscure "%{?_with_ldap: ...}" rpm macro syntax above says "if the
macro "_with_ldap" exists, then expand "...", else ignore.")

The spec file should include a default value for the "_with_ldap" macro,
and should support "--without ldap" as well. Here's a more complete example
for pine:
```
    # Default values are --without-ldap --with-ssl.
    #
    # Read: If neither macro exists, then add the default definition.
    %{!?_with_ldap: %{!?_without_ldap: %define _without_ldap --without-ldap}}
    %{!?_with_ssl: %{!?_without_ssl: %define _with_ssl --with-ssl}}
    ...

    # You might want to make sure that one and only one of required and
    # mutually exclusive options exists.
    #
    # Read: It's an error if both or neither required options exist.
    %{?_with_ssl: %{?_without_ssl: %{error: both _with_ssl and _without_ssl}}}
    %{!?_with_ssl: %{!?_without_ssl: %{error: neither _with_ssl nor _without_ssl}}}

    # Add build dependencies for ssl and ldap features if enabled.
    # Note: Tag tokens must start at beginning-of-line.
    #
    # Read: If feature is enabled, then add the build dependency.
    %{?_with_ssl:BuildRequires: openssl-devel}
    %{?_with_ldap:BuildRequires: openldap-devel}
    ...

    # Configure with desired features.
    #
    # Read: Add any defined feature values to the configure invocation.
    %configure \
	%{?_with_ssl}		\
	%{?_without_ssl}	\
	%{?_with_ldap}		\
	%{?_without_ldap}
    ...

    # Conditional tests for desired features.
    #
    # Read: true if _with_ssl is defined, false if not defined.
    %if %{?_with_ssl:1}%{!?_with_ssl:0}
    ...
    %endif

```

See also the %bcond_with and %bcond_without helper macros and their
documentation in /usr/lib/rpm/macros.
