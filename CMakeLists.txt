cmake_minimum_required(VERSION 3.20)
project(kronk3d VERSION 0.1.0 LANGUAGES CXX)

include(cmake/ProjectOptions.cmake)
include(cmake/CompilerOptions.cmake)
include(cmake/Utils.cmake)
include(cmake/Dependencies.cmake)

option(K3_BUILD_EXAMPLES "Build the kronk3d example (needs SFML, fetched if not installed)" ${PROJECT_IS_TOP_LEVEL})
option(K3_BUILD_TESTS "Build the kronk3d unit tests" ${PROJECT_IS_TOP_LEVEL})
option(K3_BUILD_GL_TESTS "Build the OpenGL backend tests (need SFML for the context)" ${K3_BUILD_TESTS})

if (K3_BUILD_EXAMPLES OR K3_BUILD_GL_TESTS)
    k3_find_sfml()
endif()

add_subdirectory(modules/math)
add_subdirectory(modules/api)
add_subdirectory(modules/logger)
add_subdirectory(modules/core)

if (K3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(modules/test)
endif()

if (K3_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

include(cmake/Install.cmake)
