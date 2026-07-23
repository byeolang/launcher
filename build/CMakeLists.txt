cmake_minimum_required (VERSION 3.14)

# include build-common if this is host project.
get_property(build_common_included GLOBAL PROPERTY BUILD_COMMON_INCLUDED)
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../external/build-common)

if(NOT build_common_included)
    include(buildCommonPre)
endif()

project(launcher C CXX)

if(NOT build_common_included)
    include(buildCommon)
    by_parse_ver("${CMAKE_CURRENT_SOURCE_DIR}/../CHANGELOGS.md")
endif()

# dependencies check:
by_check_dependency(NAMES git) # version is nothing matter.
by_check_dependency(NAMES make MIN_VERSION 3.0.0)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../module ${CMAKE_CURRENT_BINARY_DIR}/module)
