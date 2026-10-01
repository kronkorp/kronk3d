cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

# Use the system SFML when there is one (Linux packages), otherwise build it from
# source (Windows, MinGW): the example must compile without any manual install.
find_package(SFML 2.5 COMPONENTS graphics window system QUIET)

if (NOT SFML_FOUND)
    message(STATUS "SFML not found, fetching SFML 2.6.2")

    include(FetchContent)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        2.6.2
        GIT_SHALLOW    ON
        SYSTEM
    )

    set(BUILD_SHARED_LIBS OFF)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SFML)
endif()

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_executable(kronk3d_example ${SOURCES})
apply_project_settings(kronk3d_example)
target_include_directories(kronk3d_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)

# Absolute path, so the example runs from any working directory.
target_compile_definitions(kronk3d_example PRIVATE K3_EXAMPLE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

###

target_link_libraries(kronk3d_example PRIVATE kronk3d_shared sfml-graphics sfml-window sfml-system)
