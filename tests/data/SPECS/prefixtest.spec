Name:           prefixtest
Version:        1.0
Release:        1
Summary:        Testing file glob behavior
Group:          Testing
License:        GPL
BuildArch:	noarch
RemovePathPostfixes: g:f

%description
%{summary}

%define testdir /opt/%{name}

%install
mkdir -p $RPM_BUILD_ROOT/%{testdir}
echo "foo" > $RPM_BUILD_ROOT/%{testdir}/weird%%name
for f in bif baf zab zeb zib; do
    echo "$f" > $RPM_BUILD_ROOT/%{testdir}/$f
done
for f in bing bang bong; do
    mkdir -p $RPM_BUILD_ROOT/%{testdir}/$f
done
mkdir -p $RPM_BUILD_ROOT/%{testdir}/foo
for f in one two three; do
    echo "$f" > $RPM_BUILD_ROOT/%{testdir}/foo/$f
done

ln -s %{testdir}/zab $RPM_BUILD_ROOT/%{testdir}/linkgood
ln -s %{testdir}/zub $RPM_BUILD_ROOT/%{testdir}/linkbad

%files
%defattr(-,root,root,-)
%{testdir}/b??
%{testdir}/weird?name
%{testdir}/z*
%{testdir}/l*
%{testdir}/b*g/
%dir %{testdir}/foo
%{testdir}/foo/*
