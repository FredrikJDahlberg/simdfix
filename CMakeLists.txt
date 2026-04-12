cmake_minimum_required(VERSION 4.1)
project(simd_fix)

set(CMAKE_CXX_STANDARD 23)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp ${CMAKE_SOURCE_DIR}/src/test/cpp)

file(GLOB_RECURSE FIX_SOURCES "src/main/cpp/*.cpp")
file(GLOB_RECURSE FIX_HEADERS "src/main/cpp/*.h")

add_library(FixLibrary STATIC ${FIX_SOURCES} ${FIX_HEADERS})

target_include_directories(FixLibrary PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")

add_executable(TokenizerTest src/test/cpp/org/limitless/fix/parser/TokenizerTest.cpp)
target_link_libraries(TokenizerTest FixLibrary GTest::gtest GTest::gtest_main)
