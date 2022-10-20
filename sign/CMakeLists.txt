set(librpmsign_SOURCES
	rpmgensig.c
)

add_library(librpmsign SHARED)
set_target_properties(librpmsign PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_link_libraries(librpmsign PUBLIC librpmio librpm)
if (WITH_IMAEVM)
	list(APPEND librpmsign_SOURCES rpmsignfiles.c)
	target_link_libraries(librpmsign PRIVATE imaevm)
endif()
if (WITH_FSVERITY)
	list(APPEND librpmsign_SOURCES rpmsignverity.c)
	target_link_libraries(librpmsign PRIVATE ${FSVERITY_LIBRARIES})
endif()
target_sources(librpmsign PRIVATE ${librpmsign_SOURCES})

install(TARGETS librpmsign)
