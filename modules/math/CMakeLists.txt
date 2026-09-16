cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

add_library(math INTERFACE)

###

target_link_libraries(math INTERFACE KRONK3D_OPTIONS)
target_include_directories(math INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/)
