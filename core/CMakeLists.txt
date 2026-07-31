#
# Core
#

set(SOURCES
    Source/Core/Application.h
    Source/Core/Application.cpp

    Source/Core/Window.h
    Source/Core/Window.cpp

    Source/Core/Layer.h
    Source/Core/Event.h

    # Add these once they contain actual code:
    # Source/Core/Renderer/Renderer.h
    # Source/Core/Renderer/Renderer.cpp
)

add_library(Core STATIC)

target_sources(Core
    PRIVATE
        ${SOURCES}
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${SOURCES}
)

target_include_directories(Core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_link_libraries(Core
    PUBLIC
        raylib_static
        imgui
        rlimgui
)

target_compile_features(Core
    PUBLIC
        cxx_std_23
)