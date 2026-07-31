cmake_minimum_required(VERSION "4.0.0")

project(Electrical-wiring-simulator)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Raylib as static libraries" FORCE)

add_subdirectory(external/raylib-6.0)
add_subdirectory(external/imgui-docking)
add_subdirectory(external/rlImgui)

add_executable(${CMAKE_PROJECT_NAME} src/main.cpp)

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/")

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE raylib_static imgui rlimgui)


target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_23)
