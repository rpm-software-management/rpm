set(core
	elfdeps.1 gendiff.1 rpm2cpio.1 rpm2archive.1 rpmuncompress.1
	rpm.8 rpmbuild.1 rpmdb.8 rpmkeys.8 rpmsign.1 rpmspec.1
	rpmdeps.1 rpmgraph.1 rpmlua.1 rpm-common.8 rpmsort.1
	rpm-macrofile.5 rpm-config.5 rpm-rpmrc.5 rpm-setup-autosign.1
	rpm-payloadflags.7 rpmbuild-config.5 rpm-queryformat.7 rpm-macros.7
	rpm-lua.7 rpm-manifest.5 rpm-version.7 rpm-dependency-generators.7
	rpm-scriptlets.7 rpm-design.7
)
set(extra
	rpm-plugins.8 rpm-plugin-prioreset.8 rpm-plugin-syslog.8
	rpm-plugin-audit.8 rpm-plugin-dbus-announce.8
	rpm-plugin-systemd-inhibit.8 rpm-plugin-fapolicyd.8 rpm-plugin-ima.8
	rpm-plugin-selinux.8 rpm-plugin-unshare.8
)

# Build all manuals
set(manuals ${core} ${extra})
foreach(man ${manuals})
	add_custom_command(OUTPUT ${man} COMMAND
			sed '1 s/$$/ \"RPM ${CMAKE_PROJECT_VERSION}\"/'
			< ${CMAKE_CURRENT_SOURCE_DIR}/${man}.scd | ${SCDOC}
			> ${man} DEPENDS ${man}.scd)
endforeach()
add_custom_target(man ALL DEPENDS ${manuals})

# Render Jekyll-ready HTML pages from the manuals
if (SCD2HTML)
	configure_file(mkpage.in mkpage @ONLY)
	set(manuals_html "")
	foreach(man ${manuals})
		set(file ${site_dir}/man/${man}.html)
		list(APPEND manuals_html ${file})
		add_custom_command(OUTPUT ${file} COMMAND ./mkpage
				${CMAKE_CURRENT_SOURCE_DIR}/${man}.scd ${file}
				DEPENDS ${man}.scd mkpage)
	endforeach()
	add_custom_target(clidoc DEPENDS ${manuals_html})
	add_dependencies(pages clidoc)
endif()

# Install manuals for the enabled features
set(manuals ${core})
if (ENABLE_PLUGINS)
	list(APPEND manuals rpm-plugins.8
		rpm-plugin-prioreset.8 rpm-plugin-syslog.8)
	if (WITH_AUDIT)
		list(APPEND manuals rpm-plugin-audit.8)
	endif()
	if (WITH_DBUS)
		list(APPEND manuals
			rpm-plugin-dbus-announce.8 rpm-plugin-systemd-inhibit.8)
	endif()
	if (WITH_FAPOLICYD)
		list(APPEND manuals rpm-plugin-fapolicyd.8)
	endif()
	if (WITH_IMAEVM)
		list(APPEND manuals rpm-plugin-ima.8)
	endif()
	if (WITH_SELINUX)
		list(APPEND manuals rpm-plugin-selinux.8)
	endif()
	if (HAVE_UNSHARE)
		list(APPEND manuals rpm-plugin-unshare.8)
	endif()
endif()
foreach(man ${manuals})
	set(fn ${CMAKE_CURRENT_BINARY_DIR}/${man})
	get_filename_component(ext ${fn} EXT)
	string(REPLACE "." "man" section ${ext})
	install(FILES ${fn}
		DESTINATION ${CMAKE_INSTALL_MANDIR}/${section})
endforeach()
