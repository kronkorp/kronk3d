cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

# Sources are compiled once (with PIC) and shared by the static and shared flavours.
add_library(kronk3d_objects OBJECT ${SOURCES})
apply_project_settings(kronk3d_objects)
set_target_properties(kronk3d_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kronk3d_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
target_link_libraries(kronk3d_objects PUBLIC math PRIVATE logger)

add_library(kronk3d_shared SHARED $<TARGET_OBJECTS:kronk3d_objects>)
add_library(kronk3d_static STATIC $<TARGET_OBJECTS:kronk3d_objects>)

foreach(target kronk3d_shared kronk3d_static)
    apply_project_settings(${target})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
    target_link_libraries(${target} PUBLIC math PRIVATE logger)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME kronk3d)
endforeach()

# No __declspec(dllexport) annotations in the sources: let CMake export everything.
set_target_properties(kronk3d_shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# MSVC names both the static library and the DLL import library "kronk3d.lib".
if (MSVC)
    set_target_properties(kronk3d_static PROPERTIES OUTPUT_NAME kronk3d_static)
endif()

###

install(TARGETS kronk3d_shared kronk3d_static
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
# Keep the folder layout: headers include each other as "scene/Mesh.hpp", "io/ObjLoader.hpp"...
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/ ${CMAKE_CURRENT_SOURCE_DIR}/../math/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kronk3d
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
