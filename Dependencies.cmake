#
# Dependencies
#

set(BUILD_SHARED_LIBS OFF CACHE BOOL
    "Build dependencies as static libraries"
    FORCE
)

set(BUILD_EXAMPLES OFF CACHE BOOL
    "Disable dependency examples"
    FORCE
)

add_subdirectory(
    "${CMAKE_SOURCE_DIR}/external/raylib-6.0"
    "${CMAKE_BINARY_DIR}/external/raylib"
    EXCLUDE_FROM_ALL
)

add_subdirectory(
    "${CMAKE_SOURCE_DIR}/external/imgui-docking"
    "${CMAKE_BINARY_DIR}/external/imgui"
    EXCLUDE_FROM_ALL
)

add_subdirectory(
    "${CMAKE_SOURCE_DIR}/external/rlimgui"
    "${CMAKE_BINARY_DIR}/external/rlimgui"
    EXCLUDE_FROM_ALL
)