#
# Electrical Wiring Simulator application
#

set(SOURCES
    Source/Main.cpp
    Source/EditorLayer.h
    Source/EditorLayer.cpp
)

add_executable(ElectricalWiringSimulator)

target_sources(ElectricalWiringSimulator
    PRIVATE
        ${SOURCES}
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${SOURCES}
)

target_include_directories(ElectricalWiringSimulator
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_link_libraries(ElectricalWiringSimulator
    PRIVATE
        Core
)

target_compile_features(ElectricalWiringSimulator
    PRIVATE
        cxx_std_23
)

set_target_properties(
    ElectricalWiringSimulator
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/App"
)