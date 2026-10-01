cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

find_package(Threads REQUIRED)

###

# Sources are compiled once (with PIC) and shared by the static and shared flavours.
add_library(kronk3d_objects OBJECT ${SOURCES})
apply_project_settings(kronk3d_objects)
set_target_properties(kronk3d_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kronk3d_objects PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>)
target_link_libraries(kronk3d_objects PUBLIC math Threads::Threads PRIVATE logger)

add_library(kronk3d_shared SHARED $<TARGET_OBJECTS:kronk3d_objects>)
add_library(kronk3d_static STATIC $<TARGET_OBJECTS:kronk3d_objects>)

foreach(target kronk3d_shared kronk3d_static)
    apply_project_settings(${target})
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/kronk3d>
    )
    target_link_libraries(${target} PUBLIC math Threads::Threads PRIVATE logger)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME kronk3d)
endforeach()

# Exported (and aliased in-tree) as kronk3d::shared / kronk3d::static.
set_target_properties(kronk3d_shared PROPERTIES EXPORT_NAME shared)
set_target_properties(kronk3d_static PROPERTIES EXPORT_NAME static)
add_library(kronk3d::shared ALIAS kronk3d_shared)
add_library(kronk3d::static ALIAS kronk3d_static)

# No __declspec(dllexport) annotations in the sources: let CMake export everything.
set_target_properties(kronk3d_shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# MSVC names both the static library and the DLL import library "kronk3d.lib".
if (MSVC)
    set_target_properties(kronk3d_static PROPERTIES OUTPUT_NAME kronk3d_static)
endif()
