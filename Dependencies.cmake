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
    "${CMAKE_CURRENT_SOURCE_DIR}/external/raylib-6.0"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-docking"
)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/external/rlimgui"
)