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
add_executable(symbolcast-tests tests/test_symbol_match.cpp)
target_link_libraries(symbolcast-tests PRIVATE symbolcast_core)
add_test(NAME SymbolCastTests COMMAND symbolcast-tests)

enable_testing()
