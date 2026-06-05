# ── CMakeLists.txt ───────────────────────────────────────────────────────────
# Cross-platform build for the Network ML library. CMake reads this file and
# generates the right build for your platform (VS projects on Windows, Makefiles
# on Linux, etc.). It also installs the library so other CMake projects can use
# it via find_package(NetworkML).

cmake_minimum_required(VERSION 3.15)
project(NetworkML VERSION 1.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- the library target -------------------------------------------------------
add_library(networkml STATIC
    "Network ML/Requests.cpp"
    "Network ML/File.cpp"
    "Network ML/Chat.cpp"
)

# An alias so it can be linked as NetworkML::networkml whether it is used via
# add_subdirectory() or installed and found with find_package().
add_library(NetworkML::networkml ALIAS networkml)

# The header location differs between building here and being installed
# elsewhere, so describe both with generator expressions.
target_include_directories(networkml PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Network ML>
    $<INSTALL_INTERFACE:include>
)

# --- dependencies (provided by vcpkg, see vcpkg.json) -------------------------
#  - libcurl is PUBLIC: a consumer of this static lib must link it too.
#  - nlohmann_json is PRIVATE: header-only and compiled in, so consumers never
#    see it.
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(networkml
    PUBLIC  CURL::libcurl
    PRIVATE nlohmann_json::nlohmann_json
)

# --- install + export (makes find_package(NetworkML) work) --------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 1. Install the compiled library and record it in an export set.
install(TARGETS networkml
    EXPORT NetworkMLTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 2. Install the public header.
install(FILES "Network ML/networkml.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 3. Generate and install NetworkMLTargets.cmake (the importable targets).
install(EXPORT NetworkMLTargets
    FILE NetworkMLTargets.cmake
    NAMESPACE NetworkML::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkML
)

# 4. Generate the package config + version files from the template.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NetworkMLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkMLConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkML
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkMLConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkMLConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkMLConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkML
)

# ── Building, installing, and consuming ──────────────────────────────────────
#
#   Build:
#     cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#     cmake --build build --config Release
#
#   Install (to a prefix of your choice):
#     cmake --install build --prefix /path/to/install
#
#   Then, from another CMake project:
#     find_package(NetworkML REQUIRED)
#     target_link_libraries(myapp PRIVATE NetworkML::networkml)
