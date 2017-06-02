# Perl specific macro definitions.
# To make use of these macros insert the following line into your spec file:
# %include %{_rpmconfigdir}/macros.perl

#------------------------------------------------------------------------------
# Useful perl macros (from Artur Frysiak <wiget@t17.ds.pwr.wroc.pl>)
#
# For example, these can be used as (from ImageMagick.spec from PLD site)
#	[...]
#	BuildPrereq: perl
#	[...]
#	%package perl
#	Summary: libraries and modules for access to ImageMagick from perl
#	Group: Development/Languages/Perl
#	Requires: %{name} = %{version}
#	%requires_eq    perl
#	[...]
#	%install
#	rm -fr $RPM_BUILD_ROOT
#	install -d $RPM_BUILD_ROOT/%{perl_sitearch}
#	[...]
#	%files perl
#	%defattr(644,root,root,755)
#	%{perl_sitearch}/Image
#	%dir %{perl_sitearch}/auto/Image
#
%define		requires_eq()	%(LC_ALL="C" echo '%*' | xargs -r rpm -q --qf 'Requires: %%{name} = %%{epoch}:%%{version}\\n' | sed -e 's/ (none):/ /' -e 's/ 0:/ /' | grep -v "is not")
%define		perl_sitearch	%(eval "`%{__perl} -V:installsitearch`"; echo $installsitearch)
%define		perl_sitelib	%(eval "`%{__perl} -V:installsitelib`"; echo $installsitelib)
%define		perl_vendorarch %(eval "`%{__perl} -V:installvendorarch`"; echo $installvendorarch)
%define		perl_vendorlib  %(eval "`%{__perl} -V:installvendorlib`"; echo $installvendorlib)
%define		perl_archlib	%(eval "`%{__perl} -V:installarchlib`"; echo $installarchlib)
%define		perl_privlib	%(eval "`%{__perl} -V:installprivlib`"; echo $installprivlib)
