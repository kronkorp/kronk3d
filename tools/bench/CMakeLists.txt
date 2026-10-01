cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

add_executable(kronk3d_bench Main.cpp)
apply_project_settings(kronk3d_bench)
target_link_libraries(kronk3d_bench PRIVATE kronk3d::static sfml-window sfml-system)
