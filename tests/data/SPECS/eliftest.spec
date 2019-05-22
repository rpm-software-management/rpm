Name: eliftest
Version: 1.0
Release: 1
Summary: Testing %if, %elif, %else options
License: GPL
Group: Testing
BuildArch: noarch

%if  0%{?testif} == 2
    %if 0%{?rhel} == 6
Requires: pkg1
    %elif 0%{?rhel} == 7
Requires: pkg2
    %else
Requires: pkg3
    %endif
%endif

%description
%{summary}

%build
%define actecho0 exit
%define actecho1 0

%if  0%{?testif} == 2

    %if 0%{?fedora} >= 23
    %{actecho0} %{actecho1}
    %elif 0%{?fedora} >= 10 || 0%{?rhel} >= 6
    %{actecho0} %{actecho1}
    %else
    %{actecho0} %{actecho1}
    %endif

%elif 0%{testif} == 1

    %if %{variable1}
    %{action1}
    %elif %{variable2}
    %{action2}
    %elif %{variable3}
    %{action3}
    %else
    %{action4}
    %endif

%else

    %if 0
    exit 1
    %elif 0
    exit 1
    %else
    %global iftest1_macro defined
    %endif
    %{!?iftest1_macro:exit 1}

    %if 0
    exit 1
    %elif 0
    exit 1
    %elif 1
    %global iftest2_macro defined
    %elif 1
    exit 1
    %elif 0
    exit 1
    %else
    exit 1
    %endif
    %{!?iftest2_macro:exit 1}

    %if 1
    %global iftest3_macro defined
    %elif 0
    exit 1
    %elif 1
    exit 1
    %else
    exit 1
    %endif
    %{!?iftest3_macro:exit 1}

    %if 0
    exit 1
    %elif 1
    %if 0
        exit 1
    %elif 0
        exit 1
    %elif 1
        %global iftest4_macro defined
    %else
        exit 1
    %endif
    %elif 1
    exit 1
    %endif
    %{!?iftest4_macro:exit 1}

    %if 0
    exit 0
    %elif 0
    exit 0
    %elif 1
    %global iftest5_macro defined
    %elif 1
    exit 1
    %elif 0
    exit 1
    %else
    exit 1
    %endif
    %{!?iftest5_macro:exit 1}
%endif
