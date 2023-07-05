%bcond_with buildpipe
%bcond_with manyfiles

Name:		sigpipe
Version:	1.0
Release:	1
Summary:	Testing SIGPIPE behavior

Group:		Testing
License:	GPLv2
BuildArch:	noarch

%description
%{summary}
%end

%define datadir /opt/%{name}/data

# Reproducer for https://bugzilla.redhat.com/show_bug.cgi?id=651463
%if %{with buildpipe}
%prep
bash -c 's=$SECONDS
set -o pipefail
for ((i=0; i < 30; i++))
do      printf hello 2>/dev/null
        sleep .1
done |  /bin/sleep 1
(( (SECONDS-s) < 2 )) || exit 1'
%endif

%install
mkdir -p %{buildroot}/%{datadir}
%if %{with manyfiles}
for i in {1..32}; do
    mkdir %{buildroot}/%{datadir}/${i}
    for j in {a..z}; do
        touch %{buildroot}/%{datadir}/${i}/${j}
    done
done
%endif

# For simulating https://bugzilla.redhat.com/show_bug.cgi?id=1264198
%post
kill -PIPE ${PPID}

%files
%{datadir}
