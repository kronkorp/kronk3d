cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_executable(kronk3d_tests ${SOURCES})
apply_project_settings(kronk3d_tests)

###

target_link_libraries(kronk3d_tests PRIVATE kronk3d_static)

add_test(NAME kronk3d_tests COMMAND kronk3d_tests)

###

# OpenGL backend tests: need an OpenGL 3.3 context (SFML creates it). They report "skipped" when none
# can be created, unless the K3_REQUIRE_GL environment variable is set (CI with a software GL driver).
if (K3_BUILD_GL_TESTS)
    file(GLOB_RECURSE GL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/gl/*.cpp)

    add_executable(kronk3d_gl_tests ${GL_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp)
    apply_project_settings(kronk3d_gl_tests)
    target_include_directories(kronk3d_gl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)
    target_link_libraries(kronk3d_gl_tests PRIVATE kronk3d_static sfml-window sfml-system)

    add_test(NAME kronk3d_gl_tests COMMAND kronk3d_gl_tests)
endif()
