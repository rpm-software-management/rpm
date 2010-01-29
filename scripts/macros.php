# Perl specific macro definitions.
# To make use of these macros insert the following line into your spec file:
# %include %{_rpmconfigdir}/macros.php

%define		__find_requires	%{_rpmconfigdir}/find-php-requires
%define		__find_provides	%{_rpmconfigdir}/find-php-provides

%define		php_pear_dir	%{_datadir}/pear

