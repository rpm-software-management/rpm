Name:           reproducer-buildsubdir
Version:        0
Release:        0
Summary:        ...
License:        ...
BuildArch:	noarch
Buildsystem:	buildsubdir

# or any other archive
Source:         hello-2.0.tar.gz

%description
...

%conf
echo CONF: %buildsubdir

