Name:             macro-scope
Version:          1.0
Release:          1
License:          Testing
Summary:          Testing macro undefine/global behavior

%description
%{summary}


%prep
# define + undefine in a lower scoping level
%define sc12() {%{?test1: %define a01 "1"}}
%define sc11() {%{?test1: %sc12}\
		%{?test1: %undefine a01}}
%sc11

# global in a lower scoping level
%define sc22() {%{?test1: %global a02 "1"}}
%define sc21() {%{?test1: %sc22}\
		%{?test1: %undefine a02}}
%sc21

# define + undefine in a higher scoping level 1
%define sc32() {%{?test2: %undefine a03}}
%define sc31() {%{?test2: %define a03 "1"} %sc32}
%sc31

# global in a higher scoping level
%define sc42() {%{?test1: %undefine a04}}
%define sc41() {%{?test1: %global a04 "1"} %sc42}
%sc41

# define + undefine in higher scoping level 2
%define sc52() {%{?test1: %define a05 "1"}\
		%{?test1: %define a05 "2"}\
		%{?test1: %undefine a05}}
%define sc51() {%{?test1: %define a05 "3"}\
		%{?test1: %sc52}\
		%{?test1: %undefine a05}}
%sc51

# define + undefine in the same scoping level
%define a06 "1"
%undefine a06

# define + undfine in higer svoping level 3
%define sc74() {%{?test1: %define a07 "1"}}
%define sc73() {%{?test1: %sc74}}
%define sc72() {%{?test1: %sc73}\
		%{?test1: %undefine a07}}
%define sc71() {%{?test1: %sc72}}
%sc71

# define + undefine in disjoint levels 1
%define sc81() {%{?test1: %define a08 "1"}}
%sc81
%define sc82() {%{?test1: %undefine a08}}
%sc82

# define + undefine in disjoint levels 2
%define sc92() {%{?test1: %define a09 "1"}}
%define sc91() {%{?test1: %sc92}}
%sc91
%define sc94() {%{?test1: %undefine a09}}
%define sc93() {%{?test1: %sc94}}
%sc93

# define + undefine in disjoint levels 3
%define scA2() {%{?test1: %define a0A "1"}}
%define scA1() {%{?test1: %scA2}}
%define scA4() {%{?test1: %undefine a0A}}
%define scA3() {%{?test1: %scA4}}
%define scA0() {%{?test1: %scA1} %{?test1: %scA3}}
%scA0

# define + undefine in the scoping level -1
%define scB1() {%{?test1: %undefine a0B}}
%define a0B "1"
%scB1
