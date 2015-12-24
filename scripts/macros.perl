# Perl specific macro definitions.
# To make use of these macros insert the following line into your spec file:
# %include %{_rpmconfigdir}/macros.perl

%define		perl_sitearch	%(eval "`%{__perl} -V:installsitearch`"; echo $installsitearch)
%define		perl_sitelib	%(eval "`%{__perl} -V:installsitelib`"; echo $installsitelib)
%define		perl_vendorarch %(eval "`%{__perl} -V:installvendorarch`"; echo $installvendorarch)
%define		perl_vendorlib  %(eval "`%{__perl} -V:installvendorlib`"; echo $installvendorlib)
%define		perl_archlib	%(eval "`%{__perl} -V:installarchlib`"; echo $installarchlib)
%define		perl_privlib	%(eval "`%{__perl} -V:installprivlib`"; echo $installprivlib)
