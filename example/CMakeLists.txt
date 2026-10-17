cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_executable(kronk3d_example ${SOURCES})
apply_project_settings(kronk3d_example)
target_include_directories(kronk3d_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)

# Absolute path, so the example runs from any working directory.
target_compile_definitions(kronk3d_example PRIVATE K3_EXAMPLE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

###

target_link_libraries(kronk3d_example PRIVATE kronk3d_shared sfml-window sfml-system)

# Minimal glTF animation demo.
add_executable(kronk3d_animation_demo animation/Main.cpp)
apply_project_settings(kronk3d_animation_demo)
target_compile_definitions(kronk3d_animation_demo PRIVATE K3_EXAMPLE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(kronk3d_animation_demo PRIVATE kronk3d_shared sfml-window sfml-system)

# Third-person showcase: a robot in an arena (sun and shadows, sky, fog, culling, 2D overlay).
file(GLOB ARENA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/arena/*.cpp)
add_executable(kronk3d_arena ${ARENA_SOURCES})
apply_project_settings(kronk3d_arena)
target_compile_definitions(kronk3d_arena PRIVATE K3_EXAMPLE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(kronk3d_arena PRIVATE kronk3d_shared sfml-window sfml-system)
