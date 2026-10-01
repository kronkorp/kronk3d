cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_library(logger STATIC ${SOURCES})
apply_project_settings(logger)

###

# target_link_libraries(logger PRIVATE api)
target_include_directories(logger PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>)
set_target_properties(logger PROPERTIES EXPORT_NAME logger)
