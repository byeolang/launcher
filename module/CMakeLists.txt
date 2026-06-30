CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

include(FetchContent) # requires 3.14

# fetching flagStacker:
#   FetchContext could overwrites parent context(e.g. cmake variables),
#   so I have to declare mine first.
FetchContent_Declare(
    flagStacker_dep
    SOURCE_SUBDIR module 
    GIT_REPOSITORY https://github.com/byeolang/flagStacker/
    GIT_TAG v0.1.4)
FetchContent_MakeAvailable(flagStacker_dep)

# this `launcher` project generates `byeol` executable as launch my core 
# byeol interpreter.
set(moduleName "byeol") 
message("building launcher of ${moduleName}...")

foreach(d ${dependencies})
    set(libraries ${libraries} PUBLIC ${d})
endforeach(d)

message("\tbuilding ${moduleName} dependents to ${dependencies}")

#      define sources:
file(GLOB_RECURSE sources
    "*.c"
    "*.cpp"
)
#          logging sources:
foreach(e ${sources})
    message("\tembeding ${e}")
endforeach(e)
foreach(e ${dependencies})
    message("\tlinking ${e}")
endforeach(e)

set(libName "launcher")

add_library(${libName} ${sources}) # for test module
target_link_libraries(${libName}
    PRIVATE ${libraries} flagStacker)
install(TARGETS ${libName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

add_executable(${moduleName} ${sources})
target_link_libraries(${moduleName}
    PRIVATE ${libraries} flagStacker)
install(TARGETS ${moduleName} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

get_target_property(_fs_inc flagStacker
    INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "flagStacker includes = ${_fs_inc}")

#       let MSVC follow macro expansion of standard:
if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
    target_compile_options(${moduleName} PUBLIC "/Zc:preprocessor")
endif()
