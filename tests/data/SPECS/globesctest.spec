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
touch 'foo[bar]' bar baz 'foo bar' foo%%name 'docfb[123]' 'licfb[123]'
touch 'foo b' 'foo a' 'foo r'
touch 'foo & bar !'

%install
mkdir -p %{buildroot}/opt

# Glob escaping
touch '%{buildroot}/opt/foo[bar]'
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

# Space escaping
touch '%{buildroot}/opt/foo[bax bay]'
touch '%{buildroot}/opt/foo bar'
touch '%{buildroot}/opt/foo" bar"'
touch '%{buildroot}/opt/foo b'
touch '%{buildroot}/opt/foo a'
touch '%{buildroot}/opt/foo r'

# Quoting
touch '%{buildroot}/opt/foo bar.conf'
touch '%{buildroot}/opt/foo [baz]'
touch '%{buildroot}/opt/foo[bar baz]'
touch '%{buildroot}/opt/foo\[baz\]'
touch '%{buildroot}/opt/foo\bay'
touch '%{buildroot}/opt/foo"baz"'

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

# Space escaping
%doc foo\ [bar]
/opt/foo\[bax\ bay\]
/opt/foo\ bar
/opt/foo"\ bar"
/opt/foo\ [bar]

# Quoting
%config "/opt/foo bar.conf"
"/opt/foo [baz]"
"/opt/foo[bar baz]"
"/opt/foo\\[baz\\]"
"/opt/foo\\bay"
"/opt/foo\"baz\""

# Regression checks
%doc ba* "foo bar"
/opt/foo-bar*
/opt/foo?bar?baz
/opt/foo"'baz"'
/opt/foo{bar,baz}
/opt/foo{bar{a,b},baz{a,b}}
/opt/foo{bay*,baw*}

# Fallback
%doc docfb[123]
%license licfb[123]
/opt/foo[123]

# Shell control chars (#1294)
%doc "foo & bar !"
