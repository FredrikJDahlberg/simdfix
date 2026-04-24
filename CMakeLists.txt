cmake_minimum_required(VERSION 3.20)
project(simd_fix)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp ${CMAKE_SOURCE_DIR}/src/test/cpp)

set(FIX_SOURCES
        src/main/cpp/org/limitless/fix/parser/Parser.cpp
        src/main/cpp/org/limitless/fix/parser/Tokenizer.cpp)
set(FIX_HEADERS
        src/main/cpp/org/limitless/fix/parser/Tokenizer.hpp
        src/main/cpp/org/limitless/fix/parser/Utils.hpp
        src/main/cpp/org/limitless/fix/parser/Parser.hpp)

add_library(SimdFix STATIC ${FIX_SOURCES} ${FIX_HEADERS})
target_include_directories(SimdFix PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")
target_compile_options(SimdFix PRIVATE
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer -fprofile-instr-generate -fcoverage-mapping -DDEBUG_MODE>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
target_link_options(SimdFix PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>)

add_executable(TokenizerTest src/test/cpp/org/limitless/fix/parser/TokenizerTest.cpp)
target_compile_options(TokenizerTest PRIVATE -O0 -Wall -fprofile-instr-generate -fcoverage-mapping)
target_link_libraries(TokenizerTest SimdFix GTest::gtest GTest::gtest_main)
target_link_options(TokenizerTest PRIVATE $<$<CONFIG:Debug>:-fsanitize=address> -fprofile-instr-generate)

add_executable(SimdFixBench src/test/cpp/org/limitless/fix/benchmark/SimdFixBench.cpp)
target_link_libraries(SimdFixBench SimdFix)
target_compile_options(SimdFixBench PRIVATE -O3 -Wall)
target_include_directories(SimdFixBench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/test/cpp")
target_link_options(SimdFixBench PRIVATE $<$<CONFIG:Debug>:-fsanitize=address> -fprofile-instr-generate)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/include")
find_package(pugixml REQUIRED)
add_executable(Generator src/generator/org/limitless/generator/fix/Generator.cpp)
target_compile_options(Generator PRIVATE -O0 -Wall)
target_include_directories(Generator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/generator/cpp")
target_link_libraries(Generator PRIVATE pugixml::pugixml)

