set(site_dir ${CMAKE_BINARY_DIR}/site)

# Generates Jekyll source pages
add_custom_target(pages)

if (WITH_DOXYGEN)
    find_package(Doxygen REQUIRED)
endif()
if (DOXYGEN_FOUND)
	# XXX API docs should be pre-built in tarballs
	file(GLOB headers ${CMAKE_SOURCE_DIR}/include/rpm/*.h)
	set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
	set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
	set(DOXYGEN_HTML_OUTPUT ${site_dir}/api)
	if (ENABLE_WERROR)
		set(DOXYGEN_WARN_AS_ERROR YES)
	endif()
	doxygen_add_docs(apidoc librpm/Doxyheader.h ${headers}
			ALL USE_STAMP_FILE)
	add_dependencies(pages apidoc)
	install(DIRECTORY ${DOXYGEN_HTML_OUTPUT}/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/API)
endif()

set(manuals
  	manual/about.md
	manual/arch_dependencies.md
	manual/autosetup.md
	manual/boolean_dependencies.md
	manual/buildprocess.md
	manual/buildsystem.md
	manual/conditionalbuilds.md
	manual/dependencies.md
	manual/dependency_generators.md
	manual/devel_documentation.md
	manual/dynamic_specs.md
	manual/file_triggers.md
	manual/format_header.md
	manual/format_lead.md
	manual/format_v3.md
	manual/format_v4.md
	manual/format_v6.md
	manual/index.md
	manual/large_files.md
	manual/lua.md
	manual/macros.md
	manual/more_dependencies.md
	manual/philosophy.md
	manual/plugins.md
	manual/queryformat.md
	manual/relocatable.md
	manual/scriptlet_expansion.md
	manual/signatures_digests.md
	manual/spec.md
	manual/tags.md
	manual/triggers.md
	manual/tsort.md
	manual/users_and_groups.md
)
install(FILES ${manuals} TYPE DOC)

# Base files to fetch from rpm-web
set(base_files
	_layouts/default.html
	_layouts/redirected.html
)

# Local files to build with Jekyll
set(site_files
	_config.yml
	man/index.md
	man/style.css
	Gemfile
	${manuals}
)

# Configure local files
file(MAKE_DIRECTORY ${site_dir})
configure_file(index.md.in ${site_dir}/index.md @ONLY)
foreach(file ${site_files})
	configure_file(${file} ${site_dir}/${file} COPYONLY)
endforeach()

# Prepare a deployable, standalone website (if requested)
if (WITH_WEBSITE)
	# Fetch base files
	include(FetchContent)
	FetchContent_Declare(base_repo GIT_REPOSITORY
		https://github.com/rpm-software-management/rpm-web.git)
	FetchContent_MakeAvailable(base_repo)
	foreach(file ${base_files})
		configure_file(${base_repo_SOURCE_DIR}/${file}
			       ${site_dir}/${file} COPYONLY)
	endforeach()
endif()

if (WITH_WEBSITE AND PODMAN)
	option(JEKYLL_SERVE "Serve site locally when built" ON)
	set(JEKYLL_SOURCE_DIR ${site_dir} CACHE PATH "Directory to build from")

	find_program(PODMAN podman REQUIRED)
	mark_as_advanced(PODMAN)

	set(image rpm/jekyll)
	set(JEKYLL ${PODMAN} run -v ${JEKYLL_SOURCE_DIR}:/srv:z
				 -it --rm -p 4000:4000 ${image})
	if (JEKYLL_SERVE)
		set(JEKYLL ${JEKYLL} serve --host 0.0.0.0)
	else()
		set(JEKYLL ${JEKYLL} build)
	endif()

	add_custom_command(OUTPUT image
		COMMAND ${PODMAN} build -t ${image} ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND touch image
		DEPENDS Containerfile
	)
	add_custom_target(site COMMAND ${JEKYLL} DEPENDS pages image)
endif()

add_subdirectory(man)
