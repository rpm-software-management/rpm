# At least for now, this is can't be built as a standalone project. Ie,
# it can only be built as a part of rpm build tree.
add_library(rpmpgp_legacy OBJECT)

option(WITH_OPENSSL "Use openssl (instead of libgcrypt) for internal crypto" OFF)

target_sources(rpmpgp_legacy PRIVATE
	rpmpgp_internal.h rpmpgp_internal.c
)
target_include_directories(rpmpgp_legacy PRIVATE ..)
target_include_directories(rpmpgp_legacy PRIVATE ${Intl_INCLUDE_DIRS})
if (WITH_OPENSSL)
	find_package(OpenSSL 3.0.0 REQUIRED)
	target_sources(rpmpgp_legacy PRIVATE digest_openssl.c)
	target_link_libraries(rpmpgp_legacy PRIVATE OpenSSL::Crypto)
else()
	pkg_check_modules(LIBGCRYPT REQUIRED IMPORTED_TARGET libgcrypt)
	target_sources(rpmpgp_legacy PRIVATE digest_libgcrypt.c)
	target_link_libraries(rpmpgp_legacy PRIVATE PkgConfig::LIBGCRYPT)
endif()

