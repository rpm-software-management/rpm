Name: scriptfile
Version: 1
Release: 1
License: GPL
Group: Testing
Summary: Testing scriptlet file behavior
BuildArch: noarch

%description

%build

for s in verifyscript \
         pre pretrans post posttrans preun preuntrans postun postuntrans \
         triggerprein triggerin triggerun triggerpostun \
	 filetriggerin filetriggerun transfiletriggerin transfiletriggerun; do
    echo ${s} > ${s}.sh
done

%pre -f pre.sh

%pretrans -f pretrans.sh

%post -f post.sh

%posttrans -f posttrans.sh

%preun -f preun.sh

%preuntrans -f preuntrans.sh

%postun -f postun.sh

%postuntrans -f postuntrans.sh

%verifyscript -f verifyscript.sh

%triggerprein -f triggerprein.sh -- %{name}

%triggerin -f triggerin.sh -- %{name}

%triggerun -f triggerun.sh -- %{name}

%triggerpostun -f triggerpostun.sh -- %{name}

%transfiletriggerin -f transfiletriggerin.sh -- /path

%transfiletriggerun -f transfiletriggerun.sh -- /path

%filetriggerin -f filetriggerin.sh -- /path

%filetriggerun -f filetriggerun.sh -- /path

%files
