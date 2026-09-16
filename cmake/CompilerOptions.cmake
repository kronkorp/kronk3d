add_library(KRONK3D_OPTIONS INTERFACE)

target_compile_options(KRONK3D_OPTIONS INTERFACE
    ${PROJECT_WARNINGS}
    $<$<CONFIG:Debug>:${PROJECT_DEBUG_FLAGS}>
    $<$<CONFIG:Release>:${PROJECT_RELEASE_FLAGS}>
)

target_link_libraries(KRONK3D_OPTIONS INTERFACE
    stdc++exp
)

target_link_options(KRONK3D_OPTIONS INTERFACE
    -rdynamic
)

target_compile_definitions(KRONK3D_OPTIONS INTERFACE
    $<$<CONFIG:Debug>:_DEBUG>
)
