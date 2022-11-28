add_library(librpmsign SHARED)
set_target_properties(librpmsign PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_sources(librpmsign PRIVATE rpmgensig.c)

target_link_libraries(librpmsign PUBLIC librpmio librpm)
if (WITH_IMAEVM)
	target_sources(librpmsign PRIVATE rpmsignfiles.c)
	target_link_libraries(librpmsign PRIVATE imaevm)
endif()
if (WITH_FSVERITY)
	target_sources(librpmsign PRIVATE rpmsignverity.c)
	target_link_libraries(librpmsign PRIVATE PkgConfig::FSVERITY)
endif()

install(TARGETS librpmsign)
