cmake_minimum_required(VERSION 4.1)
project(simd_fix)

set(CMAKE_CXX_STANDARD 23)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp ${CMAKE_SOURCE_DIR}/src/test/cpp)

add_library(FixLibrary INTERFACE)
target_include_directories(FixLibrary INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/org/limitless/fix/parser/Tokenizer.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/org/limitless/fix/parser/Tokenizer.cpp"
)

add_executable(TokenizerTest src/test/cpp/org/limitless/fix/parser/TokenizerTest.cpp)
target_link_libraries(TokenizerTest FixLibrary GTest::gtest GTest::gtest_main)
