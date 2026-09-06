add_library(settings STATIC)

target_include_directories(settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(settings PUBLIC nlohmann_json::nlohmann_json fzy )

set_target_properties(settings PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Debug-only test-path override: only active when SETTINGS_DEBUG_TEST_PATH cache variable is set.
if(SETTINGS_DEBUG_TEST_PATH)
    target_compile_definitions(settings PUBLIC SETTINGS_DEBUG_TEST_PATH)
endif()

# Coverage support (requires gcovr + LLVM coverage tools)
# Change to use moden msvc coverage tools if needed
option(BUILD_COVERAGE "Enable code coverage instrumentation" OFF)
if(BUILD_COVERAGE)
    target_link_options(settings PUBLIC /PROFILE)
endif()

target_sources(settings PRIVATE
    fzy.hpp
    fzy.cpp
    settings_manager.hpp
    settings_manager.cpp
)
