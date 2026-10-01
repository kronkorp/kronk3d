cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_executable(kronk3d_example ${SOURCES})
apply_project_settings(kronk3d_example)
target_include_directories(kronk3d_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)

# Absolute path, so the example runs from any working directory.
target_compile_definitions(kronk3d_example PRIVATE K3_EXAMPLE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

###

target_link_libraries(kronk3d_example PRIVATE kronk3d_shared sfml-window sfml-system)
