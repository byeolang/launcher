CMAKE_MINIMUM_REQUIRED(VERSION 3.14)
include(FetchContent) # requires 3.14

# googletest (v1.17.0). gtest_force_shared_crt keeps gtest on the dynamic CRT
# (/MD) so it matches the launcher's runtime library on Windows and avoids an
# LNK2038 RuntimeLibrary mismatch.
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

# the test executable is named `test` so it lands at bin/test, which is what
# `builder.py test` runs.
set(moduleName "test")
message("building ${moduleName}...")

file(GLOB_RECURSE sources "*.cc")
foreach(e ${sources})
    message("\tembeding ${e}")
endforeach(e)

add_executable(${moduleName} ${sources})

# mock/curlMock.cc reaches the linker as an object file and libcurl as an archive
# behind launcherLib, so the fake wins and curl.cpp runs against it unmodified.
target_link_libraries(${moduleName}
    PRIVATE launcherLib GTest::gmock GTest::gtest_main Threads::Threads)

target_include_directories(${moduleName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(${moduleName})

# let MSVC follow standard macro expansion (matches the launcher target)
if((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    target_compile_options(${moduleName} PUBLIC "/Zc:preprocessor")
endif()
