cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

# Header-only for now (Registry.hpp is a template, nothing to compile).
# The day this module gets its first .cpp, switch to the math/logger pattern:
#   file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
#   add_library(api STATIC ${SOURCES})
#   apply_project_settings(api)
# and PUBLIC/PRIVATE below instead of INTERFACE.
add_library(api INTERFACE)

###

target_link_libraries(api INTERFACE KRONK3D_OPTIONS)
target_include_directories(api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/)

###

# target_link_libraries(api INTERFACE math)
