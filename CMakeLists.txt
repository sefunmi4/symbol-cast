cmake_minimum_required(VERSION 3.14)
project(SymbolCast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library (header-only)
add_library(symbolcast_core INTERFACE)
target_include_directories(symbolcast_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Desktop application
add_executable(symbolcast-desktop apps/desktop/main.cpp)
target_link_libraries(symbolcast-desktop PRIVATE symbolcast_core)

# VR application
add_executable(symbolcast-vr apps/vr/main.cpp)
target_link_libraries(symbolcast-vr PRIVATE symbolcast_core)

# Tests
add_executable(test_symbol_match tests/test_symbol_match.cpp)
target_link_libraries(test_symbol_match PRIVATE symbolcast_core)
add_test(NAME TestSymbolMatch COMMAND test_symbol_match)

add_executable(test_double_tap tests/test_double_tap.cpp)
target_link_libraries(test_double_tap PRIVATE symbolcast_core)
add_test(NAME TestDoubleTap COMMAND test_double_tap)

add_executable(test_command_mapping tests/test_command_mapping.cpp)
target_link_libraries(test_command_mapping PRIVATE symbolcast_core)
add_test(NAME TestCommandMapping COMMAND test_command_mapping)

enable_testing()
