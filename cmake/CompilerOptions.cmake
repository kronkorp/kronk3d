add_library(KRONK3D_OPTIONS INTERFACE)
set_target_properties(KRONK3D_OPTIONS PROPERTIES EXPORT_NAME options)

target_compile_options(KRONK3D_OPTIONS INTERFACE
    ${PROJECT_WARNINGS}
    $<$<CONFIG:Debug>:${PROJECT_DEBUG_FLAGS}>
    $<$<CONFIG:Release>:${PROJECT_RELEASE_FLAGS}>
)

target_compile_definitions(KRONK3D_OPTIONS INTERFACE
    $<$<CONFIG:Debug>:_DEBUG>
)

if (WIN32)
    target_compile_definitions(KRONK3D_OPTIONS INTERFACE
        NOMINMAX                  # <windows.h> must not break std::min / std::max
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS   # fopen & co. (stb_image, logger)
    )
endif()

# Exports every symbol to the dynamic table so backtraces can resolve names (ELF only).
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(KRONK3D_OPTIONS INTERFACE
        -rdynamic
    )
endif()
