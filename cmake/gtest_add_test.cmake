# gtest_add_test.cmake
# Registers a Unity test classes as CTest tests with DEF_SOURCE_LINE for VS Code Test Explorer.
#
# Usage:
#   gtest_add_test(
#     TARGET <executable_target_name>
#     SOURCES <source_file1.cpp> [<source_file2.cpp> ...]
#     PREFIX <prefix_string>       # optional, e.g. "backend.core"
#   )
#
# The function:
#   1. Reads each source file to find test class declarations (class XxxTest : public QObject).
#   2. Within each class, finds private slots methods matching void test_*( ).
#   3. Registers one CTest test per test method with DEF_SOURCE_LINE set to <absolute_path>:<line>.

function(find_cpp_method_definitions _file_path _out_results_var)
    if(NOT EXISTS "${_file_path}")
        message(FATAL_ERROR "File not found: ${_file_path}")
    endif()

    # file(STRINGS) safely isolates each line regardless of double-quotes or semicolons inside C++ code
    file(STRINGS "${_file_path}" _lines)

    set(_current_line_num 0)
    set(_matches "")

    # Cleaned regex pattern matching TEST, TEST_F, TEST_P
    set(_method_pattern "^[ \t]*TEST(_F|_P)?[ \t]*\\([ \t]*([a-zA-Z0-9_]+)[ \t]*,[ \t]*([a-zA-Z0-9_]+)[ \t]*\\)")

    foreach(_line IN LISTS _lines)
        math(EXPR _current_line_num "${_current_line_num} + 1")

        if(_line MATCHES "${_method_pattern}")
            # CMAKE_MATCH_2 is Suite Name, CMAKE_MATCH_3 is Test Name
            set(_suite_name "${CMAKE_MATCH_2}")
            set(_test_name "${CMAKE_MATCH_3}")
            list(APPEND _matches "${_current_line_num}:${_suite_name}.${_test_name}")
        endif()
    endforeach()

    set(${_out_results_var} "${_matches}" PARENT_SCOPE)
endfunction()

function(gtest_add_test)
  cmake_parse_arguments(
    GT               # prefix
    ""               # options
    "SOURCE;PREFIX"  # one_value_keywords
    ""               # multi_value_keywords
    ${ARGN}
  )

  if(NOT GT_SOURCE OR GT_SOURCE STREQUAL "")
    message(FATAL_ERROR "gtest_add_test: SOURCE is required and must not be empty")
  endif()

  if(NOT GT_PREFIX)
    set(GT_PREFIX "")
  endif()

  # we want to construct the test target name as GT_PREFIX_SOURCE_FILENAME without dots 
  # and without extension, so we can use it as a CTest test name
  get_filename_component(file_stem "${GT_SOURCE}" NAME_WE)
  string(CONCAT GT_PREFIX "${GT_PREFIX}" "." "${file_stem}")
  string(REPLACE "." "_" test_target "${GT_PREFIX}")
  # message(STATUS "gtest_add_test: Registering tests from ${GT_SOURCE} in target ${test_target} with prefix '${GT_PREFIX}'")

  add_executable(${test_target} ${GT_SOURCE})
  target_include_directories(${test_target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(${test_target} PRIVATE GTest::gtest settings)
  set_target_properties(${test_target} PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
  )

  # we need the source files to be absolute paths for the DEF_SOURCE_LINE property to work correctly
  if(NOT IS_ABSOLUTE "${GT_SOURCE}")
    get_filename_component(GT_SOURCE "${GT_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  if(NOT EXISTS "${GT_SOURCE}")
    message(FATAL_ERROR "gtest_add_test: Source file does not exist: ${GT_SOURCE}")
  endif()

  find_cpp_method_definitions("${GT_SOURCE}" my_methods)

  foreach(_entry IN LISTS my_methods)
    if(_entry MATCHES "^([0-9]+):([a-zA-Z][a-zA-Z0-9_]*)\\.([a-zA-Z][a-zA-Z0-9_]*)")
      set(_line_num "${CMAKE_MATCH_1}")
      set(_gtest_suit "${CMAKE_MATCH_2}")
      set(_gtest_test "${CMAKE_MATCH_3}")

      set(_test_name "${_gtest_suit}.${_gtest_test}")

      # Add CTest test with DEF_SOURCE_LINE for VS Code Test Explorer integration
      add_test(
        NAME ${_test_name}
        COMMAND ${test_target} "--gtest_filter=${_test_name}"
      )
      # message(STATUS "NAME ${_test_name}, COMMAND: ${test_target} --gtest_filter=${_gtest_filter}")

      set_property(TEST ${_test_name} PROPERTY DEF_SOURCE_LINE "${GT_SOURCE}:${_line_num}")
    endif()
  endforeach()
endfunction()
