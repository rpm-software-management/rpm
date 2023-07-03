find_program(PODMAN podman docker REQUIRED)

add_custom_target(build_image COMMAND
		${PODMAN} build -t rpm -f ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile ${CMAKE_SOURCE_DIR})

add_custom_target(ci COMMAND
		${PODMAN} run -t rpm
		DEPENDS build_image)

