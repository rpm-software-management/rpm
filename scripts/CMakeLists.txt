set(rpmconfig_SCRIPTS
	brp-compress brp-strip brp-strip-comment-note
	brp-strip-static-archive brp-elfperms brp-remove-la-files
	check-files check-prereqs
	check-buildroot check-rpaths check-rpaths-worker
	find-lang.sh find-requires find-provides
	perl.prov perl.req
	pkgconfigdeps.sh
	ocamldeps.sh
	fontconfig.prov script.req
	rpm_macros_provides.sh
	rpmdb_dump rpmdb_load
	rpm2cpio.sh tgpg
)

set(rpmconfig_DATA
	rpm.daily rpm.log rpm.supp
)

install(PROGRAMS gendiff TYPE BIN)
install(PROGRAMS ${rpmconfig_SCRIPTS} DESTINATION ${RPMCONFIGDIR})
install(FILES ${rpmconfig_DATA} DESTINATION ${RPMCONFIGDIR})
