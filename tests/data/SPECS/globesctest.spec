# We need to hardcode the install prefix so that we can compare the %%doc
# filenames in the resulting package (with "rpm -qpl") against a static list in
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
touch 'foo[bar]' bar baz 'foo bar' 'foo b' 'foo a' 'foo r'

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

# Space escaping
touch '%{buildroot}/opt/foo[bax bay]'
touch '%{buildroot}/opt/foo bar'
touch '%{buildroot}/opt/foo" bar"'
touch '%{buildroot}/opt/foo b'
touch '%{buildroot}/opt/foo a'
touch '%{buildroot}/opt/foo r'

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

%files

%doc foo\[bar\] ba* "foo bar" foo\ [bar]

# Glob escaping
/opt/foo\[bar\]
"/opt/foo\[bar baz\]"
/opt/foo\\\[bar\\\]
/opt/foo\*
/opt/foo\\bar
/opt/foo\\
/opt/foo\?bar
/opt/foo\{bar,baz\}
/opt/foo\"bar\"

# Space escaping
/opt/foo\[bax\ bay\]
/opt/foo\ bar
/opt/foo"\ bar"
/opt/foo\ [bar]

# Regression checks
/opt/foo-bar*
/opt/foo?bar?baz
/opt/foo"'baz"'
/opt/foo{bar,baz}
/opt/foo{bar{a,b},baz{a,b}}
/opt/foo{bay*,baw*}
