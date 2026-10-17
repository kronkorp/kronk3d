cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_library(kronk3d_logger STATIC ${SOURCES})
apply_project_settings(kronk3d_logger)

###

# XX
target_include_directories(kronk3d_logger PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>)
set_target_properties(kronk3d_logger PROPERTIES EXPORT_NAME logger)
