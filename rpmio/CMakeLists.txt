add_library(librpmio SHARED)

target_sources(librpmio PRIVATE
	argv.cc base64.cc digest.cc expression.cc macro.cc rpmhook.hh rpmhook.cc
	rpmio.cc rpmlog.cc rpmmalloc.cc rgetopt.cc rpmpgp.cc rpmpgpval.hh
	rpmsq.cc rpmsw.cc url.cc rpmio_internal.hh rpmvercmp.cc
	rpmver.cc rpmstring.cc rpmfileutil.cc rpmglob.cc rpmkeyring.cc
	rpmstrpool.cc rpmmacro_internal.hh rpmlua.cc rpmlua.hh lposix.cc
	progname.cc
)
target_compile_definitions(librpmio PRIVATE RPM_CONFIGDIR="${RPM_CONFIGDIR}")
target_include_directories(librpmio 
    PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${Intl_INCLUDE_DIRS}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (EXISTS ${CMAKE_SOURCE_DIR}/rpmio/rpmpgp_legacy/CMakeLists.txt)
	add_subdirectory(rpmpgp_legacy)
endif()

if (WITH_SEQUOIA)
	pkg_check_modules(RPMSEQUOIA REQUIRED IMPORTED_TARGET rpm-sequoia>=1.9.0)
	target_sources(librpmio PRIVATE rpmpgp_sequoia.cc)
	target_link_libraries(librpmio PRIVATE PkgConfig::RPMSEQUOIA)
else()
	if (WITH_LEGACY_OPENPGP)
		target_link_libraries(librpmio PRIVATE rpmpgp_legacy)
	else()
		target_sources(librpmio PRIVATE rpmpgp_dummy.cc)
	endif()
	if (WITH_OPENSSL)
		find_package(OpenSSL 1.0.2 REQUIRED)
		target_sources(librpmio PRIVATE digest_openssl.cc)
		target_link_libraries(librpmio PRIVATE OpenSSL::Crypto)
	else()
		pkg_check_modules(LIBGCRYPT REQUIRED IMPORTED_TARGET libgcrypt)
		target_sources(librpmio PRIVATE digest_libgcrypt.cc)
		target_link_libraries(librpmio PRIVATE PkgConfig::LIBGCRYPT)
	endif()
endif()

set_target_properties(librpmio PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_link_libraries(librpmio PRIVATE
	PkgConfig::POPT
	LUA::LUA
	ZLIB::ZLIB
	${Intl_LIBRARIES}
)

if (ZSTD_FOUND)
	target_link_libraries(librpmio PRIVATE PkgConfig::ZSTD)
endif()
if (LIBLZMA_FOUND)
	target_link_libraries(librpmio PRIVATE PkgConfig::LIBLZMA)
endif()
if (BZIP2_FOUND)
	target_link_libraries(librpmio PRIVATE BZip2::BZip2)
endif()
if (OpenMP_C_FOUND)
        target_link_libraries(librpmio PRIVATE OpenMP::OpenMP_C)
endif()

install(TARGETS librpmio EXPORT rpm-targets)
