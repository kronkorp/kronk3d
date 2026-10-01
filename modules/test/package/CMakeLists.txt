# Builds against an *installed* kronk3d, the way another project would (checked in CI):
#     cmake -S modules/test/package -B consumer -DCMAKE_PREFIX_PATH=<install prefix> -DK3_LINK=static|shared
cmake_minimum_required(VERSION 3.20)
project(kronk3d_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(K3_LINK static CACHE STRING "kronk3d flavour to link: static or shared")

find_package(kronk3d 0.1 REQUIRED)

add_executable(kronk3d_consumer main.cpp)
target_link_libraries(kronk3d_consumer PRIVATE kronk3d::${K3_LINK})
