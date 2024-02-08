%bcond clang 0
%define debug_package %{nil}

Name: bpf
Version: 1.0
Release: 1
Summary: Testing bpf ELF file behavior
BuildArch: noarch
License: GPL

%description
%{summary}

%prep
%setup -c -T

%build
%if %{with clang}
cat << EOF > bpf.c
int func(void)
{
    return 0;
}
EOF

clang -target bpf -c bpf.c -o bpf.o
%else
cp /data/misc/bpf.o .
%endif

%install
mkdir -p ${RPM_BUILD_ROOT}/opt/
install -m755 bpf.o ${RPM_BUILD_ROOT}/opt/

%files
/opt/bpf.o
