cmake_minimum_required(VERSION 3.18)
project(trpm VERSION 1.0 DESCRIPTION "test rpm plugin API" LANGUAGES C)
find_package(rpm REQUIRED)

set(CMAKE_SHARED_MODULE_PREFIX "")

add_library(debug MODULE debug.c)
target_link_libraries(debug PRIVATE rpm::librpm)
install(TARGETS debug DESTINATION ${RPM_PLUGINDIR})
install(FILES macros.transaction_debug DESTINATION ${RPM_MACROSDIR})
