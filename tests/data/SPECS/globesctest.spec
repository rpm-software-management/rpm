# Make sure %%doc files are installed into a hard-coded prefix so that we can
# then compare them against a static list using "rpm -qpl" in
# tests/rpmbuild.at.
%global _prefix /opt

Name:           globesctest
Version:        1.0
Release:        1
Summary:        Testing file glob escape behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}.


%build
touch 'foo[bar]' bar baz 'foo bar' foo%%name 'foo[123]'

%install
mkdir -p %{buildroot}/opt

# Glob escaping
touch '%{buildroot}/opt/foo[bar]'
touch '%{buildroot}/opt/foo[bar baz]'
touch '%{buildroot}/opt/foo\[bar\]'
touch '%{buildroot}/opt/foo*'
touch '%{buildroot}/opt/foo\bar'
touch %{buildroot}/opt/foo\\
touch '%{buildroot}/opt/foo?bar'
touch '%{buildroot}/opt/foo{bar,baz}'
touch '%{buildroot}/opt/foo"bar"'
touch '%{buildroot}/opt/foo*b'
touch '%{buildroot}/opt/foo*a'
touch '%{buildroot}/opt/foo*r'

# Macro escaping
touch '%{buildroot}/opt/foo%%name'

# Regression checks
touch '%{buildroot}/opt/foo-bar1'
touch '%{buildroot}/opt/foo-bar2'
touch '%{buildroot}/opt/fooxbarybaz'
touch "%{buildroot}/opt/foo\"'baz\"'"
touch '%{buildroot}/opt/foobar'
touch '%{buildroot}/opt/foobaz'
touch '%{buildroot}/opt/foobara'
touch '%{buildroot}/opt/foobarb'
touch '%{buildroot}/opt/foobaza'
touch '%{buildroot}/opt/foobazb'
touch '%{buildroot}/opt/foobaya'
touch '%{buildroot}/opt/foobayb'
touch '%{buildroot}/opt/foobawa'
touch '%{buildroot}/opt/foobawb'

# Fallback
touch '%{buildroot}/opt/foo[123]'

%files

# Glob escaping
%doc foo\[bar\]
/opt/foo\[bar\]
/opt/foo\*[bar]
"/opt/foo\[bar baz\]"
/opt/foo\\\[bar\\\]
/opt/foo\*
/opt/foo\\bar
/opt/foo\\
/opt/foo\?bar
/opt/foo\{bar,baz\}
/opt/foo\"bar\"

# Macro escaping
%doc foo%%name
/opt/foo%%name

# Regression checks
%doc ba* "foo bar"
/opt/foo-bar*
/opt/foo?bar?baz
/opt/foo"'baz"'
/opt/foo{bar,baz}
/opt/foo{bar{a,b},baz{a,b}}
/opt/foo{bay*,baw*}

# Fallback
%doc foo[123]
/opt/foo[123]
