cmake_minimum_required(VERSION 3.14)
project(SymbolCast LANGUAGES CXX)

if(APPLE)
  enable_language(OBJC)
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum macOS version")
  endif()
endif()

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

option(SC_ENABLE_TROCR "Enable TrOCR decoding with LibTorch" OFF)
if(SC_ENABLE_TROCR)
  find_package(Torch REQUIRED)
  find_path(TOKENIZERS_INCLUDE_DIR tokenizers_cpp/tokenizers.h
            HINTS $ENV{TOKENIZERS_ROOT}
            PATH_SUFFIXES include include/tokenizers_cpp)
  find_library(TOKENIZERS_LIBRARY NAMES tokenizers_cpp tokenizers
               HINTS $ENV{TOKENIZERS_ROOT}
               PATH_SUFFIXES lib lib64)
  if(NOT TOKENIZERS_INCLUDE_DIR OR NOT TOKENIZERS_LIBRARY)
    message(FATAL_ERROR "tokenizers C++ library not found. Set TOKENIZERS_ROOT to its prefix.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
  target_include_directories(symbolcast_core INTERFACE ${TORCH_INCLUDE_DIRS} ${TOKENIZERS_INCLUDE_DIR})
  target_compile_definitions(symbolcast_core INTERFACE SC_ENABLE_TROCR)
endif()

# Desktop application
# MSVC and some other toolchains don't always link Qt dependencies
# transitively, leading to "unresolved external symbol" (LNK2001) errors.
# Link Core and Gui explicitly alongside Widgets to ensure all required
# Qt libraries are included.
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets)
if(QT_FOUND)
  set(CMAKE_AUTOMOC ON)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

  set(symbolcast_desktop_sources
      apps/desktop/main.cpp
      apps/desktop/CanvasWindow.hpp)

  if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "SymbolCast")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.symbolcast.desktop")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "1.0")
    set(MACOSX_BUNDLE_EXECUTABLE_NAME "symbolcast-desktop")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/apps/desktop/Info.plist.in
                   ${CMAKE_CURRENT_BINARY_DIR}/symbolcast-desktop-Info.plist @ONLY)
  endif()

  add_executable(symbolcast-desktop
      MACOSX_BUNDLE
      ${symbolcast_desktop_sources})
  target_link_libraries(symbolcast-desktop PRIVATE
      symbolcast_core
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Widgets)
  if(APPLE)
    set_target_properties(symbolcast-desktop PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/symbolcast-desktop-Info.plist)
  endif()

  if(APPLE)
    add_executable(symbolcast-service-macos
        MACOSX_BUNDLE
        os/service-macos/main.m
        os/service-macos/ServiceDelegate.m
        os/service-macos/ServiceDelegate.h)
    set_target_properties(symbolcast-service-macos PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/os/service-macos/Info.plist
        OUTPUT_NAME symbolcast-service-macos)
    target_link_libraries(symbolcast-service-macos PRIVATE "-framework Cocoa")
    add_dependencies(symbolcast-desktop symbolcast-service-macos)
  endif()
  if(SC_ENABLE_TROCR)
    target_link_libraries(symbolcast-desktop PRIVATE ${TORCH_LIBRARIES} ${TOKENIZERS_LIBRARY})
    add_executable(symbolcast-trocr-infer
        apps/trocr_infer.cpp)
    target_link_libraries(symbolcast-trocr-infer PRIVATE
        symbolcast_core
        Qt${QT_VERSION_MAJOR}::Core)
    target_link_libraries(symbolcast-trocr-infer PRIVATE ${TORCH_LIBRARIES} ${TOKENIZERS_LIBRARY})
  endif()
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

add_executable(test_model_loading tests/test_model_loading.cpp)
target_link_libraries(test_model_loading PRIVATE symbolcast_core)
add_test(NAME TestModelLoading COMMAND test_model_loading)

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
