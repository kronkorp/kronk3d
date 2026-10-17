cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(kronk3d_math STATIC ${SOURCES})
apply_project_settings(kronk3d_math)

###

target_include_directories(kronk3d_math PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/kronk3d>
)
set_target_properties(kronk3d_math PROPERTIES EXPORT_NAME math)
add_library(kronk3d::math ALIAS kronk3d_math)
