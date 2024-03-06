install(PROGRAMS gendiff TYPE BIN)
install(PROGRAMS
	brp-compress brp-strip brp-strip-comment-note
	brp-strip-static-archive brp-elfperms brp-remove-la-files
	check-files check-prereqs
	check-buildroot check-rpaths check-rpaths-worker
	find-lang.sh find-requires find-provides
	pkgconfigdeps.sh
	ocamldeps.sh
	fontconfig.prov script.req
	rpm_macros_provides.sh
	rpmdb_dump rpmdb_load
	rpm2cpio.sh tgpg
	sysusers.sh
	DESTINATION ${RPM_CONFIGDIR}
)
install(FILES
	rpm.daily rpm.log rpm.supp
	DESTINATION ${RPM_CONFIGDIR}
)
