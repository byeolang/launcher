CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

message("building ${CMAKE_CURRENT_SOURCE_DIR}...")

add_subdirectory(launcher)
IF(NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_subdirectory(test)
ENDIF(NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
