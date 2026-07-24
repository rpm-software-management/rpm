%bcond setup 0

Name:           readin
Version:        1.0
Release:        1
Summary:        Simple read-in test package
Group:		Testing
License:        GPL
BuildArch:	noarch
Source:		source-noroot.tar.gz

%description
%{summary}

%if %{with setup}
%prep
%setup -C
%endif

%build
cat << EOF > file1
#!/bin/sh
echo yay
EOF
chmod a+x file1
cp file1 file2

cat << EOF > $RPM_READINS_DIR/manifest1.list
/opt/bin/file1
EOF

cat << EOF > manifest2.list
/opt/bin/file2
EOF

cat << EOF > %{readinsdir}/post.sh
#!/bin/sh
echo post
EOF
chmod a+x $RPM_READINS_DIR/post.sh

%install
mkdir -p $RPM_BUILD_ROOT/opt/bin
cp file1 file2 $RPM_BUILD_ROOT/opt/bin/

%post -f post.sh

%files -f manifest1.list -f manifest2.list
%defattr(-,root,root,-)
