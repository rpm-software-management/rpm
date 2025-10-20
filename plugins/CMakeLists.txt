add_library(prioreset MODULE prioreset.c)
add_library(syslog MODULE syslog.c)

if(WITH_SELINUX)
	add_library(selinux MODULE selinux.c)
	target_link_libraries(selinux PRIVATE PkgConfig::SELINUX)
endif()

if(WITH_DBUS)
	pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)
	pkg_get_variable(dbus-1_DATADIR dbus-1 datadir)
	# Comply with CMP0177
	cmake_path(SET DBUS_DATADIR NORMALIZE "${dbus-1_DATADIR}")
	add_library(systemd_inhibit MODULE systemd_inhibit.c)
	target_link_libraries(systemd_inhibit PRIVATE PkgConfig::DBUS)
	add_library(dbus_announce MODULE dbus_announce.c)
	target_link_libraries(dbus_announce PRIVATE PkgConfig::DBUS)
	install(FILES
	  org.rpm.conf
	  DESTINATION ${DBUS_DATADIR}/dbus-1/system.d/
	)
endif()

if(WITH_IMAEVM)
	add_library(ima MODULE ima.c)
endif()

if(WITH_FAPOLICYD)
	add_library(fapolicyd MODULE fapolicyd.c)
endif()

if(WITH_AUDIT)
	add_library(audit MODULE audit.c)
	target_link_libraries(audit PRIVATE PkgConfig::AUDIT)
endif()

if(WITH_FSVERITY)
	add_library(fsverity MODULE fsverity.c)
	target_link_libraries(fsverity PRIVATE PkgConfig::FSVERITY)
	target_include_directories(fsverity PRIVATE ${CMAKE_SOURCE_DIR}/sign)
endif()

if (HAVE_UNSHARE)
	add_library(unshare MODULE unshare.c)
endif()

get_property(plugins DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(plugin ${plugins})
	target_link_libraries(${plugin} PRIVATE librpmio librpm ${Intl_LIBRARIES})
	target_include_directories(${plugin} PRIVATE ${Intl_INCLUDE_DIRS})
	install(TARGETS ${plugin} DESTINATION ${RPM_PLUGINDIR})
	install(FILES macros.transaction_${plugin} DESTINATION ${RPM_MACROSDIR})
endforeach()

