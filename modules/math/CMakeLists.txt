cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(math STATIC ${SOURCES})
apply_project_settings(math)

###

target_include_directories(math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
