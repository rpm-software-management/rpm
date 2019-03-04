%define mymacro0 \
%if 0  \
%global bbb0 ccc0\
Summary: macro 0\
%endif \
%{nil}

%define mymacro1 \
%if 1  \
%global bbb1 ccc1 \
Summary: macro 1\
%endif
%{nil}

Name:           iftest
Version:        1.0
Release:        1
Group:          Testing
License:        GPL
BuildArch:      noarch

%mymacro0
%mymacro1

%description
%bbb0
%bbb1
