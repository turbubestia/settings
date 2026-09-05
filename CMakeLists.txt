cmake_minimum_required(VERSION 3.21)

project(settings 
    VERSION 0.1.0
    DESCRIPTION "A lightweight, JSON-backed C++ settings manager with built-in fuzzy search."
    LANGUAGES C CXX
)

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist/${CMAKE_BUILD_TYPE}" CACHE PATH "Install prefix" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add custom CMake module path for qtest_add_test and other helpers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Library dependencies
find_package(GTest CONFIG REQUIRED)
find_package(fzy REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Source folders
add_subdirectory(src)

# Test configuration
include(CTest)
enable_testing()
add_subdirectory(tests)
