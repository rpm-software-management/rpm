set(fileattrs_DATA
	debuginfo.attr desktop.attr elf.attr font.attr metainfo.attr
	perl.attr perllib.attr pkgconfig.attr ocaml.attr
	rpm_macro.attr script.attr)

install(FILES ${fileattrs_DATA} DESTINATION ${RPMCONFIGDIR}/fileattrs)
