# Global settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

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
