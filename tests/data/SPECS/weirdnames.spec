%bcond_with illegal

Name:		weirdnames
Version:	1.0
Release:	1
Summary:	Testing weird filename behavior
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
mkdir -p %{buildroot}/opt
cd %{buildroot}/opt
touch "foo'ed" 'bar"ed' "just space" "\$talks" \
	"to?be" "the * are falling" '#nocomment' "(maybe)" \
	"perhaps;not" "true:false" "!absolutely" "&ground" \
	"after{all}" "index[this]" "equals=not" "tee|two" "~right" \
	"<namehere>"
%if %{with illegal}
touch "only	time"
# the dependency generator cannot handle newlines in filenames
touch "new
line"

%endif
for f in *; do
    echo -e "#!/bin/ary\ndada\n" > ${f}
done
chmod a+x *

# script.req fails on the backslash
touch "\.back"

%files
/opt/*
