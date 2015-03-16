Summary:	testdoc
Name:		testdoc
Version:	1.0
Release:	1
Group:		Utilities
License:	GPLv2+
Distribution: RPM test suite.
BuildArch:  noarch

%description
Package for testing rpm install option "--excludedocs".

%install
mkdir -p %{buildroot}%{_datadir}/testdoc
echo "nodoc" > %{buildroot}%{_datadir}/testdoc/nodoc

mkdir -p %{buildroot}%{_docdir}/testdoc
echo "doc1" > %{buildroot}%{_docdir}/testdoc/documentation1
echo "doc2" > %{buildroot}%{_docdir}/testdoc/documentation2

mkdir -p %{buildroot}%{_docdir}/testdoc/examples
echo "example1" > %{buildroot}%{_docdir}/testdoc/examples/example1
echo "example2" > %{buildroot}%{_docdir}/testdoc/examples/example2

%files
%{_datadir}/testdoc/nodoc
%dir %{_docdir}/testdoc
%doc %{_docdir}/testdoc/documentation1
%doc %{_docdir}/testdoc/documentation2
%dir %{_docdir}/testdoc/examples
%doc %{_docdir}/testdoc/examples/example1
%doc %{_docdir}/testdoc/examples/example2
