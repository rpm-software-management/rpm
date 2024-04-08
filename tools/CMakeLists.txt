add_library(cliutils OBJECT cliutils.c cliutils.h)

add_executable(rpm rpm.c cliutils)
add_executable(rpmdb rpmdb.c cliutils)
add_executable(rpmkeys rpmkeys.c cliutils)
add_executable(rpmsign rpmsign.c cliutils)
add_executable(rpmbuild rpmbuild.c cliutils)
add_executable(rpmspec rpmspec.c cliutils)

add_executable(rpmdeps rpmdeps.c)
add_executable(rpmgraph rpmgraph.c)
add_executable(rpmlua rpmlua.c)
add_executable(rpmuncompress rpmuncompress.c)
add_executable(rpmdump rpmdump.c)

target_link_libraries(rpmsign PRIVATE librpmsign)
target_link_libraries(rpmlua PRIVATE LUA::LUA)
target_link_libraries(rpmbuild PRIVATE librpmbuild)
target_link_libraries(rpmspec PRIVATE librpmbuild)
target_link_libraries(rpmdeps PRIVATE librpmbuild)
target_link_libraries(rpmuncompress PRIVATE PkgConfig::LIBARCHIVE)

if (HAVE_STRCHRNUL)
	add_executable(rpmsort rpmsort.c)
	install(TARGETS rpmsort)
endif()

if (LIBELF_FOUND)
	add_executable(elfdeps elfdeps.c)
	target_link_libraries(elfdeps PRIVATE PkgConfig::LIBELF)
	install(TARGETS elfdeps DESTINATION ${RPM_CONFIGDIR})
endif()

target_include_directories(rpmlua PRIVATE ${CMAKE_SOURCE_DIR}/rpmio)
target_include_directories(rpmgraph PRIVATE ${CMAKE_SOURCE_DIR}/lib)

if (READLINE_FOUND)
	target_link_libraries(rpmspec PRIVATE PkgConfig::READLINE)
	target_link_libraries(rpmlua PRIVATE PkgConfig::READLINE)
endif()

if (WITH_ARCHIVE)
	add_executable(rpm2archive rpm2archive.c)
	target_link_libraries(rpm2archive PRIVATE PkgConfig::LIBARCHIVE)
	install(TARGETS rpm2archive)
endif()

# Everything links to these
get_property(executables DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(exe ${executables})
	target_link_libraries(${exe} PRIVATE librpmio librpm PkgConfig::POPT)
	target_link_libraries(${exe} PRIVATE ${Intl_LIBRARIES})
	target_include_directories(${exe} PRIVATE ${Intl_INCLUDE_DIRS})
endforeach()

foreach(cmd rpmverify rpmquery)
	add_custom_target(${cmd} ALL COMMAND
			${CMAKE_COMMAND} -E create_symlink rpm ${cmd}
			)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${cmd} TYPE BIN)
endforeach()
if (WITH_ARCHIVE)
	add_custom_target(rpm2cpio ALL COMMAND
			${CMAKE_COMMAND} -E create_symlink rpm2archive rpm2cpio
			)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rpm2cpio TYPE BIN)
endif()

if (WITH_CXX)
set (cxx_sources
	cliutils.c elfdeps.c rpm2archive.c rpmbuild.c rpm.c rpmdb.c
	rpmdeps.c rpmdump.c rpmgraph.c rpmkeys.c rpmlua.c rpmsign.c
	rpmsort.c rpmspec.c
)
set_source_files_properties(${cxx_sources} PROPERTIES LANGUAGE CXX)
endif()

install(TARGETS
	rpm rpmdb rpmkeys rpmsign rpmbuild rpmspec
	rpmlua rpmgraph
)
install(TARGETS rpmdeps rpmdump rpmuncompress DESTINATION ${RPM_CONFIGDIR})

