# Global settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Executables and shared libraries land in the same folder: on Windows the
# example can only find kronk3d.dll if it sits right next to it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if (MSVC)
    # Warnings
    set(PROJECT_WARNINGS
        /W4 /permissive- /utf-8 /Zc:__cplusplus
    )

    # Build flags
    set(PROJECT_DEBUG_FLAGS /Od /Zi)
    set(PROJECT_RELEASE_FLAGS /O2)
else()
    # Warnings
    set(PROJECT_WARNINGS
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnull-dereference
        -Wcast-align -Wmissing-declarations
        -Wundef -Wunreachable-code
    )

    # Build flags
    set(PROJECT_DEBUG_FLAGS -g -O0)
    set(PROJECT_RELEASE_FLAGS -O3 -g)
endif()
