cmake_minimum_required(VERSION 3.16)
project(kronk3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB_RECURSE KRONK3D_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE KRONK3D_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

add_library(kronk3d_shared SHARED ${KRONK3D_SOURCES} ${KRONK3D_HEADERS})
add_library(kronk3d_static STATIC ${KRONK3D_SOURCES} ${KRONK3D_HEADERS})

foreach(target kronk3d_shared kronk3d_static)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set_target_properties(${target} PROPERTIES OUTPUT_NAME kronk3d)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_libraries(${target} PUBLIC stdc++exp)
    endif()
endforeach()

set_target_properties(kronk3d_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(TARGETS kronk3d_shared kronk3d_static
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION include)
