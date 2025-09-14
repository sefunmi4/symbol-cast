cmake_minimum_required(VERSION 3.14)
project(SymbolCast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library (header-only)
add_library(symbolcast_core INTERFACE)
target_include_directories(symbolcast_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

option(SC_USE_ONNXRUNTIME "Enable ONNX Runtime" OFF)
if(SC_USE_ONNXRUNTIME)
  find_path(ONNXRUNTIME_INCLUDE onnxruntime_cxx_api.h PATHS $ENV{ONNXRUNTIME_ROOT}/include)
  find_library(ONNXRUNTIME_LIB onnxruntime PATHS $ENV{ONNXRUNTIME_ROOT}/lib)
  if(NOT ONNXRUNTIME_INCLUDE OR NOT ONNXRUNTIME_LIB)
    message(FATAL_ERROR "ONNX Runtime not found")
  endif()
  target_include_directories(symbolcast_core INTERFACE ${ONNXRUNTIME_INCLUDE})
  target_link_libraries(symbolcast_core INTERFACE ${ONNXRUNTIME_LIB})
  target_compile_definitions(symbolcast_core INTERFACE SC_USE_ONNXRUNTIME)
endif()

# Desktop application
# Explicitly link against Qt Core and Gui along with Widgets. MSVC and
# other toolchains may not pull these libraries in transitively, which
# triggers a flood of "unresolved external symbol" (e.g. LNK2001) errors
# during linking. Listing them here ensures all required Qt libraries are
# linked explicitly.
set(QT_COMPONENTS Core Gui Widgets)
find_package(QT NAMES Qt6 Qt5 COMPONENTS ${QT_COMPONENTS})
if(QT_FOUND)
  set(CMAKE_AUTOMOC ON)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMPONENTS})

  add_executable(symbolcast-desktop
      apps/desktop/main.cpp
      apps/desktop/CanvasWindow.hpp)
  target_link_libraries(symbolcast-desktop PRIVATE symbolcast_core)
  foreach(component ${QT_COMPONENTS})
    target_link_libraries(symbolcast-desktop PRIVATE Qt${QT_VERSION_MAJOR}::${component})
  endforeach()
endif()

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

add_executable(test_timestamp tests/test_timestamp.cpp)
target_link_libraries(test_timestamp PRIVATE symbolcast_core)
add_test(NAME TestTimestamp COMMAND test_timestamp)

add_executable(test_vr_capture tests/test_vr_capture.cpp)
target_link_libraries(test_vr_capture PRIVATE symbolcast_core)
add_test(NAME TestVRCapture COMMAND test_vr_capture)

add_executable(test_dynamic_recognizer tests/test_dynamic_recognizer.cpp)
target_link_libraries(test_dynamic_recognizer PRIVATE symbolcast_core)
add_test(NAME TestDynamicRecognizer COMMAND test_dynamic_recognizer)

add_executable(test_hybrid_recognizer tests/test_hybrid_recognizer.cpp)
target_link_libraries(test_hybrid_recognizer PRIVATE symbolcast_core)
add_test(NAME TestHybridRecognizer COMMAND test_hybrid_recognizer)

enable_testing()
