cmake_minimum_required(VERSION 4.0.0)

project(
    ElectricalWiringSimulator
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(Dependencies.cmake)

add_subdirectory(Core)
add_subdirectory(App)