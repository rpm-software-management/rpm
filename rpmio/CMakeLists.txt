add_library(librpmio SHARED)

target_sources(librpmio PRIVATE
	argv.c base64.c digest.c expression.c macro.c rpmhook.h rpmhook.c
	rpmio.c rpmlog.c rpmmalloc.c rgetopt.c rpmpgp.c rpmpgpval.h
	rpmsq.c rpmsw.c url.c rpmio_internal.h rpmvercmp.c
	rpmver.c rpmstring.c rpmfileutil.c rpmglob.c rpmkeyring.c
	rpmstrpool.c rpmmacro_internal.h rpmlua.c rpmlua.h lposix.c
)

if (WITH_INTERNAL_OPENPGP)
	target_sources(librpmio PRIVATE rpmpgp_internal.h rpmpgp_internal.c)
	if (WITH_OPENSSL)
		find_package(OpenSSL REQUIRED)
		target_sources(librpmio PRIVATE digest_openssl.c)
		target_link_libraries(librpmio PRIVATE OpenSSL:Crypto)
	else()
		pkg_check_modules(LIBGCRYPT REQUIRED IMPORTED_TARGET libgcrypt)
		target_sources(librpmio PRIVATE digest_libgcrypt.c)
		target_link_libraries(librpmio PRIVATE PkgConfig::LIBGCRYPT)
	endif()
else()
	pkg_check_modules(RPMSEQUOIA REQUIRED IMPORTED_TARGET rpm-sequoia)
	target_sources(librpmio PRIVATE rpmpgp_sequoia.c)
	target_link_libraries(librpmio PRIVATE PkgConfig::RPMSEQUOIA)
endif()

set_target_properties(librpmio PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_link_libraries(librpmio PRIVATE
			PkgConfig::POPT
			PkgConfig::LUA
			ZLIB::ZLIB)

if (ZSTD_FOUND)
	target_link_libraries(librpmio PRIVATE PkgConfig::ZSTD)
endif()
if (LIBLZMA_FOUND)
	target_link_libraries(librpmio PRIVATE PkgConfig::LIBLZMA)
endif()
if (BZIP2_FOUND)
	target_link_libraries(librpmio PRIVATE BZip2::BZip2)
endif()

install(TARGETS librpmio
)
