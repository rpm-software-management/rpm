Name: scriptfile
Version: 1
Release: 1
License: GPL
Group: Testing
Summary: Testing scriptlet file behavior
BuildArch: noarch

%description

%build
cat << EOF > one.sh
#!/bin/sh
echo one
EOF

cat << EOF > two.sh
#!/bin/sh
echo two
EOF

%pre -f one.sh

%postun -f two.sh

%triggerin -f one.sh -- %{name}

%triggerun -f two.sh -- %{name}

%transfiletriggerin -f one.sh -- /path

%transfiletriggerun -f two.sh -- /path

%filetriggerin -f one.sh -- /path

%filetriggerin -f two.sh -- /path

%files
