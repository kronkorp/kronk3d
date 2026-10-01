cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

include(cmake/ProjectOptions.cmake)
include(cmake/CompilerOptions.cmake)
include(cmake/Utils.cmake)

option(K3_BUILD_EXAMPLES "Build the kronk3d example (needs SFML, fetched if not installed)" ${PROJECT_IS_TOP_LEVEL})

add_subdirectory(modules/math)
add_subdirectory(modules/api)
add_subdirectory(modules/logger)
add_subdirectory(modules/core)

if (K3_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
