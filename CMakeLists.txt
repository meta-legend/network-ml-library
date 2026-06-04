# ── CMakeLists.txt ───────────────────────────────────────────────────────────
# A cross-platform description of HOW to build the Network ML library.
# CMake reads this file and generates the right build files for whatever
# platform you're on (Visual Studio projects on Windows, Makefiles on Linux,
# Xcode on macOS). The same file builds everywhere.

# 1. Minimum CMake version this file relies on. 3.15+ is a safe modern baseline.
cmake_minimum_required(VERSION 3.15)

# 2. Name the project and say it uses C++ (CXX). This also defines variables
#    like ${PROJECT_NAME} that you can reuse below.
project(NetworkML VERSION 1.6 LANGUAGES CXX)

# 3. Require C++17 for the whole project (std::filesystem, etc.).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. Declare the library target and list the source files that make it up.
#    STATIC = produce a .lib/.a (the kind you ship). Use SHARED for a .dll/.so.
add_library(networkml STATIC
    "Network ML/Requests.cpp"
    "Network ML/File.cpp"
    "Network ML/Chat.cpp"
)

# 5. Include directories.
#    PUBLIC = applies to this library AND anything that links it (consumers
#             need to find networkml.h, so it's PUBLIC).
target_include_directories(networkml
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Network ML"
)

# 6. Dependencies, both provided by vcpkg (see vcpkg.json).
#    - libcurl is PUBLIC: consumers link a static lib that references curl
#      symbols, so the dependency must propagate to them.
#    - nlohmann_json is PRIVATE: it's a header-only implementation detail used
#      only inside Chat.cpp; consumers never see it.
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(networkml
    PUBLIC  CURL::libcurl
    PRIVATE nlohmann_json::nlohmann_json
)

# ── How to build with this file ──────────────────────────────────────────────
#
#   Configure (generate build files), pointing at vcpkg for libcurl:
#     cmake -B build -S . ^
#       -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
#       -DVCPKG_TARGET_TRIPLET=x64-windows-static
#
#   Compile:
#     cmake --build build --config Release
#
# On Linux/macOS you'd drop the vcpkg line (libcurl comes from the system) and
# just run the same two commands — that's the whole point of CMake.
