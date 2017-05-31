Name:		filedep
Version:	1.0
Release:	1
Summary:	Testing file dependencies
License:	GPL
BuildArch:	noarch

%description
%{summary}

%prep
%setup -q -T -c %{name}-%{version}


%build
cat << EOF >> foo
#!/bin/sh
cat /etc/foo.conf
EOF

cat << EOF >> bar
#!/bin/f00f
echo BUG
EOF

cat << EOF >> foo.conf
hello there
EOF

cat << EOF >> README
Some stuff, huh?
EOF

%install
mkdir -p %{buildroot}/etc
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/doc/filedep
install -D -m 644 foo.conf %{buildroot}/etc
install -D -m 755 foo bar %{buildroot}/usr/bin/
install -D -m 644 README %{buildroot}/usr/share/doc/filedep

%files
/etc/*
/usr/bin/*
/usr/share/doc/filedep/


%changelog

