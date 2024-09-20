Name: filetypes
Version: 1.0
Release: 1
Summary: Testing different file types
License: Public domain
BuildArch: noarch

%description
%{summary}

%install
mkdir -p %{buildroot}/opt
mkdir -p %{buildroot}/opt/mydir
echo "some text" > %{buildroot}/opt/README
cat << EOF > %{buildroot}/opt/myscript.sh
#!/bin/sh
echo some script
EOF
ln -s myscript.sh %{buildroot}/opt/linkme

%files
/opt/mydir
/opt/README
/opt/myscript.sh
/opt/linkme
