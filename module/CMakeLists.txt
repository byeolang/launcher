CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

include(FetchContent) # requires 3.14

# flagStacker
FetchContent_Declare(
    flagStacker_dep
    SOURCE_SUBDIR module
    GIT_REPOSITORY https://github.com/byeolang/flagStacker/
    GIT_TAG v0.1.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(flagStacker_dep)

# minizip-ng (ZIP extraction, bundles zlib automatically)
set(MZ_ZLIB             ON  CACHE BOOL "" FORCE)
set(MZ_BZIP2            OFF CACHE BOOL "" FORCE)
set(MZ_LZMA             OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD             OFF CACHE BOOL "" FORCE)
set(MZ_PKCRYPT          OFF CACHE BOOL "" FORCE)
set(MZ_WZAES            OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL          OFF CACHE BOOL "" FORCE)
set(MZ_LIBBSD           OFF CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS       ON  CACHE BOOL "" FORCE)
set(MZ_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    minizip_dep
    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
    GIT_TAG 4.0.7
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(minizip_dep)
FetchContent_GetProperties(minizip_dep)

# curl (static, platform-native SSL)
set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE        OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL       OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2      OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP     ON  CACHE BOOL "" FORCE)
if(WIN32)
    set(CURL_USE_SCHANNEL     ON CACHE BOOL "" FORCE)
elseif(APPLE)
    set(CURL_USE_SECTRANSP    ON CACHE BOOL "" FORCE)
else()
    set(CURL_USE_OPENSSL      ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
    curl_dep
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_11_0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(curl_dep)

# this `launcher` project generates `byeol` executable as launch my core
# byeol interpreter.
set(moduleName "byeol")
message("building launcher of ${moduleName}...")

foreach(d ${dependencies})
    set(libraries ${libraries} PUBLIC ${d})
endforeach(d)

message("\tbuilding ${moduleName} dependents to ${dependencies}")

file(GLOB_RECURSE sources "*.cpp")
foreach(e ${sources})
    message("\tembeding ${e}")
endforeach(e)
foreach(e ${dependencies})
    message("\tlinking ${e}")
endforeach(e)

set(libName "launcher")

add_library(${libName} ${sources})
target_include_directories(${libName} PRIVATE ${minizip_dep_SOURCE_DIR})
target_link_libraries(${libName}
    PRIVATE ${libraries} flagStacker MINIZIP::minizip CURL::libcurl)
install(TARGETS ${libName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

add_executable(${moduleName} ${sources})
target_include_directories(${moduleName} PRIVATE ${minizip_dep_SOURCE_DIR})
target_link_libraries(${moduleName}
    PRIVATE ${libraries} flagStacker MINIZIP::minizip CURL::libcurl)
install(TARGETS ${moduleName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

get_target_property(_fs_inc flagStacker
    INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "flagStacker includes = ${_fs_inc}")

if((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    target_compile_options(${moduleName} PUBLIC "/Zc:preprocessor")
endif()
