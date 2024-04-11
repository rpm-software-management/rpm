Name: caps
Version: 1.0
Release: 1
Summary: Test %caps
License: Public domain

%description
%{summary}

%prep
cat << EOF > test.c
int main(int argc, char *argv[])
{
	return 0;
}
EOF

%build
gcc -o test test.c

%install
install -m 755 test -D ${RPM_BUILD_ROOT}/usr/bin/test

%files
%caps(cap_net_raw=p) /usr/bin/test
