cmake_minimum_required( VERSION 3.12 )

project(spb-proto VERSION 1.0.0 LANGUAGES CXX)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SPB_PROTO_BUILD_TESTS "Build tests" OFF)
option(SPB_PROTO_BUILD_COMPATIBILITY_TESTS "Build gpb compatibility tests" OFF)
option(SPB_PROTO_BUILD_COMPILER_TESTS "Build sprotoc tests" OFF)
option(SPB_PROTO_BUILD_ETL_TESTS "Build etl compatibility tests" OFF)
option(SPB_PROTO_BUILD_FUZZER_TESTS "Build fuzzers for spb-protoc, json-parser, gpb-parser" OFF)
option(SPB_PROTO_BUILD_EXAMPLES "Build examples" OFF)
option(SPB_PROTO_USE_CLANG_FORMAT "Enable clang format for generated code" ON)
option(SPB_PROTO_USE_COVERAGE "Enable code coverage" OFF)
option(SPB_PROTO_USE_ADDRESS_SANITIZER "Enable address sanitizer" OFF)
option(SPB_PROTO_USE_UB_SANITIZER "Enable undefined behavior sanitizer" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/spb_protobuf.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/spb_compile_options.cmake)

# the main serialization library is header only
add_library(spb-proto INTERFACE)
target_include_directories(spb-proto INTERFACE include)
target_compile_features(spb-proto INTERFACE cxx_std_20)

add_subdirectory(src)

if(SPB_PROTO_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(SPB_PROTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()