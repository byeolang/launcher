CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

include(FetchContent) # requires 3.14

# flagStacker
FetchContent_Declare(
    flagStacker_dep
    SOURCE_SUBDIR module
    GIT_REPOSITORY https://github.com/byeolang/flagStacker/
    GIT_TAG v0.1.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(flagStacker_dep)

# minizip-ng (ZIP extraction, bundles zlib automatically)
# We only static-link these deps into byeol; we never `install` them. Skipping
# their install rules also sidesteps a minizip-ng bug where it tries to
# install() the fetched zlib-ng `zlib` ALIAS target (fatal on Linux/Windows,
# where MZ_ZLIB stays on; macOS avoids it via Apple libcompression).
set(SKIP_INSTALL_ALL    ON  CACHE BOOL "" FORCE)
set(MZ_ZLIB             ON  CACHE BOOL "" FORCE)
set(MZ_BZIP2            OFF CACHE BOOL "" FORCE)
set(MZ_LZMA             OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD             OFF CACHE BOOL "" FORCE)
set(MZ_PKCRYPT          OFF CACHE BOOL "" FORCE)
set(MZ_WZAES            OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL          OFF CACHE BOOL "" FORCE)
set(MZ_LIBBSD           OFF CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS       ON  CACHE BOOL "" FORCE)
set(MZ_FORCE_FETCH_LIBS ON  CACHE BOOL "" FORCE)
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
set(USE_LIBIDN2           OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP     ON  CACHE BOOL "" FORCE)
# We download release zips as-is and unzip them with minizip; curl never needs
# zlib for HTTP content-decoding. Turning it off keeps zlib-ng (linked by
# minizip) the single, statically-embedded zlib in the binary and avoids
# dragging in a dynamic system libz through curl.
set(CURL_ZLIB             OFF CACHE STRING "" FORCE)
if(WIN32)
    set(CURL_USE_SCHANNEL     ON CACHE BOOL "" FORCE)
elseif(APPLE)
    set(CURL_USE_SECTRANSP    ON CACHE BOOL "" FORCE)
else()
    # Linux: static-link OpenSSL so the launcher doesn't depend on the target
    # machine's libssl/libcrypto soname at runtime. Requires libssl-dev (which
    # ships libssl.a/libcrypto.a) on the build machine.
    set(CURL_USE_OPENSSL      ON CACHE BOOL "" FORCE)
    set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
    curl_dep
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_11_0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(curl_dep)

# this `launcher` project builds the `launcher` executable, whose user-facing
# output file is named `byeol` (project/target name = launcher, shipped file =
# byeol). A `launcherLib` static library of the same sources is kept so unit
# tests can link the launcher logic directly.
set(exeName    "launcher")   # project/target name (internal identifier)
set(outputName "byeol")      # user-facing file name
set(libName    "launcherLib")
message("building ${exeName} (ships as ${outputName})...")

foreach(d ${dependencies})
    set(libraries ${libraries} PUBLIC ${d})
endforeach(d)

message("\tbuilding ${exeName} dependents to ${dependencies}")

# non-recursive on purpose: the launcher sources live at module/ top level;
# GLOB_RECURSE would also pull in module/test/*.cpp (which needs gtest).
file(GLOB sources "*.cpp")
foreach(e ${sources})
    message("\tembeding ${e}")
endforeach(e)
foreach(e ${dependencies})
    message("\tlinking ${e}")
endforeach(e)

# test-facing static library of the launcher logic
add_library(${libName} ${sources})
target_include_directories(${libName} PRIVATE ${minizip_dep_SOURCE_DIR})
target_link_libraries(${libName}
    PRIVATE ${libraries} flagStacker MINIZIP::minizip CURL::libcurl)
install(TARGETS ${libName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

# the launcher executable; target is `launcher`, output file is `byeol`
add_executable(${exeName} ${sources})
set_target_properties(${exeName} PROPERTIES OUTPUT_NAME ${outputName})
target_include_directories(${exeName} PRIVATE ${minizip_dep_SOURCE_DIR})
target_link_libraries(${exeName}
    PRIVATE ${libraries} flagStacker MINIZIP::minizip CURL::libcurl)
install(TARGETS ${exeName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

get_target_property(_fs_inc flagStacker
    INTERFACE_INCLUDE_DIRECTORIES)

if((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    target_compile_options(${exeName} PUBLIC "/Zc:preprocessor")
endif()
