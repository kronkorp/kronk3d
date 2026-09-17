cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_executable(kronk3d_example ${SOURCES})
apply_project_settings(kronk3d_example)

###

target_link_libraries(kronk3d_example PRIVATE kronk3d_shared sfml-graphics sfml-window sfml-system)
