cmake_minimum_required(VERSION 3.20)
project(simd_fix)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp ${CMAKE_SOURCE_DIR}/src/test/cpp)

set(FIX_SOURCES
        src/main/cpp/org/limitless/fix/parser/Tokenizer.cpp
)
set(FIX_HEADERS
        src/main/cpp/org/limitless/fix/parser/Tokenizer.hpp
)

add_library(SimdFix STATIC ${FIX_SOURCES} ${FIX_HEADERS})
target_compile_options(SimdFix PRIVATE
        -O3
        -mcpu=apple-m1
        -Wall
)
target_include_directories(SimdFix PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")

add_executable(TokenizerTest src/test/cpp/org/limitless/fix/parser/TokenizerTest.cpp)
target_link_libraries(TokenizerTest SimdFix GTest::gtest GTest::gtest_main)

add_executable(SimdFixBench src/test/cpp/org/limitless/fix/benchmark/SimdFixBench.cpp)
target_compile_options(SimdFixBench PRIVATE
        -O3
        -Wall
)
target_link_libraries(SimdFixBench SimdFix)
target_include_directories(SimdFixBench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/test/cpp")
