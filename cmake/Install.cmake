# `cmake --install build --prefix <dir>` installs the libraries, the headers (folder layout kept:
# they include each other as "scene/Mesh.hpp"...) and a CMake package, so that other projects can do
#     find_package(kronk3d 0.1 REQUIRED)
#     target_link_libraries(app PRIVATE kronk3d::static)   # or kronk3d::shared
include(CMakePackageConfigHelpers)

set(K3_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/kronk3d)

install(TARGETS kronk3d_shared kronk3d_static math logger KRONK3D_OPTIONS
    EXPORT kronk3dTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/modules/core/src/ ${PROJECT_SOURCE_DIR}/modules/math/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kronk3d
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
install(EXPORT kronk3dTargets NAMESPACE kronk3d:: DESTINATION ${K3_CMAKE_DIR})

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/kronk3dConfig.cmake.in
    ${PROJECT_BINARY_DIR}/kronk3dConfig.cmake
    INSTALL_DESTINATION ${K3_CMAKE_DIR}
)
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/kronk3dConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(FILES ${PROJECT_BINARY_DIR}/kronk3dConfig.cmake ${PROJECT_BINARY_DIR}/kronk3dConfigVersion.cmake
    DESTINATION ${K3_CMAKE_DIR}
)
