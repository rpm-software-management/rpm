%global __os_install_post %{nil}
Name: manualdebug
Version: 0.1
Release: 1
License: GPL
Summary: Testing manual debug_package use

%description
%{summary}

%prep
cat << EOF > main.c
#include <stdio.h>
int main(int argc, char *argv[])
{
	printf("hello\n");
	return 0;
}
EOF

%build
gcc -g main.c

%install
mkdir -p $RPM_BUILD_ROOT/usr/bin
cp a.out $RPM_BUILD_ROOT/usr/bin/hello-world

%files
/usr/bin/hello-world

%debug_package
