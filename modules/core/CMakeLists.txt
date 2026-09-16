cmake_minimum_required(VERSION 3.20)
project(kronk3d LANGUAGES CXX)

###

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

###

add_library(kronk3d_shared SHARED ${SOURCES})
add_library(kronk3d_static STATIC ${SOURCES})

foreach(target kronk3d_shared kronk3d_static)
    apply_project_settings(${target})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME kronk3d)
endforeach()

set_target_properties(kronk3d_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

###

target_link_libraries(kronk3d_shared PRIVATE math logger)
target_link_libraries(kronk3d_static PRIVATE math logger)

###

file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)

install(TARGETS kronk3d_shared kronk3d_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kronk3d)
