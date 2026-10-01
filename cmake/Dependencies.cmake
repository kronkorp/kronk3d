# SFML is only used by the example and the OpenGL tests, to open a window / an OpenGL context: the
# library itself depends on nothing. Use the system SFML when there is one (Linux packages), otherwise
# build it from source (Windows, MinGW), so nothing has to be installed by hand.
function(k3_find_sfml)
    if (TARGET sfml-window)
        return()
    endif()

    find_package(SFML 2.5 COMPONENTS window system QUIET)
    if (SFML_FOUND)
        return()
    endif()

    message(STATUS "SFML not found, fetching SFML 2.6.2")
    include(FetchContent)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        2.6.2
        GIT_SHALLOW    ON
        SYSTEM
    )

    set(BUILD_SHARED_LIBS OFF)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_GRAPHICS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SFML)
endfunction()
