set(SOURCES
Application.h
Application.cpp
Window.h
Window.cpp
Layer.h
#Source/Core/Renderer/Renderer.h
#Source/Core/Renderer/Renderer.cpp
#Source/Core/Renderer/Shader.h
#ource/Core/Renderer/Shader.cpp
#Source/Core/Renderer/GLUtils.h
#Source/Core/Renderer/GLUtils.cpp
)

add_library(Core STATIC)

target_sources(Core PRIVATE ${SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

add_compile_definitions(
    GLFW_INCLUDE_NONE
)

#target_link_libraries(Core glfw)
#target_link_libraries(Core glad)
#target_link_libraries(Core glm)

target_include_directories(Core PUBLIC "Source")