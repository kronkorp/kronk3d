cmake_minimum_required(VERSION 3.20)
project(raytracer LANGUAGES CXX)

include(cmake/ProjectOptions.cmake)
include(cmake/CompilerOptions.cmake)
include(cmake/Utils.cmake)

# add_subdirectory(modules/math)
# add_subdirectory(modules/api)
add_subdirectory(modules/logger)
add_subdirectory(modules/core)
