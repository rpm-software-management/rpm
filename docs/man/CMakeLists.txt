
set(manuals
	gendiff.1 rpm2cpio.8
	rpm.8 rpmbuild.8 rpmdb.8 rpmkeys.8 rpmsign.8 rpmspec.8
	rpmdeps.8 rpmgraph.8 rpmlua.8 rpm-misc.8
)

if (WITH_ARCHIVE)
	list(APPEND manuals rpm2archive.8)
endif()

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
	if (WITH_IMA)
		list(APPEND manuals rpm-plugin-ima.8)
	endif()
	if (WITH_SELINUX)
		list(APPEND manuals rpm-plugin-selinux.8)
	endif()
endif()

foreach(man ${manuals})
	# XXX manuals should be pre-built in tarballs
	if (DEFINED PANDOC)
		add_custom_target(${man} ALL COMMAND ${PANDOC}
				${CMAKE_CURRENT_SOURCE_DIR}/${man}.md -o ${man})
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${man} TYPE MAN)
	endif()
endforeach()
