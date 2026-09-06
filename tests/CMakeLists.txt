
# Register tests with CTest + VS Code Test Explorer integration.
include(${CMAKE_SOURCE_DIR}/cmake/gtest_add_test.cmake)

# gtest_add_test(SOURCE fzy_test.cpp PREFIX settings)
gtest_add_test(SOURCE setting_manager_test.cpp PREFIX settings)
