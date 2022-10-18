set(_rpm_SOURCES
	rpmmodule.c rpmsystem-py.h
	header-py.c header-py.h
	rpmarchive-py.c rpmarchive-py.h
	rpmds-py.c rpmds-py.h
	rpmfd-py.c rpmfd-py.h
	rpmfiles-py.c rpmfiles-py.h
	rpmkeyring-py.c rpmkeyring-py.h
	rpmmi-py.c rpmmi-py.h
	rpmii-py.c rpmii-py.h
	rpmps-py.c rpmps-py.h
	rpmmacro-py.c rpmmacro-py.h
	rpmstrpool-py.c rpmstrpool-py.h
	rpmtd-py.c rpmtd-py.h
	rpmte-py.c rpmte-py.h
	rpmts-py.c rpmts-py.h
	rpmver-py.c rpmver-py.h
	spec-py.c spec-py.h
)
	
Python3_add_library(_rpm ${_rpm_SOURCES})
target_link_libraries(_rpm PRIVATE librpmio librpm librpmbuild librpmsign)

install(TARGETS _rpm
	DESTINATION ${Python3_SITEARCH}/rpm)
install(FILES rpm/transaction.py rpm/__init__.py
	DESTINATION ${Python3_SITEARCH}/rpm)

set(egginfo ${PROJECT_NAME}-${PROJECT_VERSION}-py${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}.egg-info)
configure_file(rpm.egg-info.in ${egginfo} @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${egginfo}
	DESTINATION ${Python3_SITEARCH})
