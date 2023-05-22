add_library(prioreset MODULE prioreset.c)
add_library(syslog MODULE syslog.c)

if(WITH_SELINUX)
	add_library(selinux MODULE selinux.c)
	target_link_libraries(selinux PRIVATE PkgConfig::SELINUX)
endif()

if(WITH_DBUS)
	pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)
	add_library(systemd_inhibit MODULE systemd_inhibit.c)
	target_link_libraries(systemd_inhibit PRIVATE PkgConfig::DBUS)
	add_library(dbus_announce MODULE dbus_announce.c)
	target_link_libraries(dbus_announce PRIVATE PkgConfig::DBUS)
	install(FILES
	  org.rpm.conf
	  DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/dbus-1/system.d/
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
endif()

set(plugindir ${CMAKE_INSTALL_FULL_LIBDIR}/rpm-plugins)

get_property(plugins DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(plugin ${plugins})
	target_link_libraries(${plugin} PRIVATE librpmio librpm)
	install(TARGETS ${plugin} DESTINATION ${plugindir})
endforeach()

