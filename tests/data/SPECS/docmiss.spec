Name:           docmiss
Version:        1.0
Release:        1
Summary:        Testing missing doc behavior
Group:          Testing
License:        GPL

%description
%{summary}


%prep
%setup  -c -n %{name}-%{version} -T

cat << EOF > COPYING
This is not a license
EOF

cat << EOF > README
This is a dog project
EOF

%files
%doc CREDITS COPYING README
