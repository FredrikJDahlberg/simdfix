cmake_minimum_required(VERSION 3.20)
project(simd_fix)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp ${CMAKE_SOURCE_DIR}/src/test/cpp)

set(FIX_SOURCES
        src/main/cpp/org/limitless/fix/parser/Decoder.cpp
        src/main/cpp/org/limitless/fix/parser/Tokenizer.cpp)
set(FIX_HEADERS
        src/main/cpp/org/limitless/fix/parser/Tokenizer.hpp
        src/main/cpp/org/limitless/fix/parser/Utils.hpp
        src/main/cpp/org/limitless/fix/parser/Decoder.hpp
        src/main/cpp/org/limitless/fix/parser/BitSet64.hpp
        src/main/cpp/org/limitless/fix/parser/MessageDecoder.hpp
        src/main/cpp/org/limitless/fix/messages/LogonDecoder.hpp
        src/main/cpp/org/limitless/fix/messages/MessageHandler.hpp
        src/main/cpp/org/limitless/fix/parser/ParserStatus.hpp
        src/main/cpp/org/limitless/fix/messages/MessageTypes.hpp
        src/main/cpp/org/limitless/fix/parser/Dictionary.hpp
        src/main/cpp/org/limitless/fix/messages/Grammar.hpp
        src/main/cpp/org/limitless/fix/parser/GroupDecoder.hpp
        src/main/cpp/org/limitless/fix/parser/PerfectHashMap.hpp)

add_library(SimdFix STATIC ${FIX_SOURCES} ${FIX_HEADERS})
target_include_directories(SimdFix PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")
target_compile_options(SimdFix PRIVATE
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer -fprofile-instr-generate -fcoverage-mapping -DDEBUG_MODE>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
target_link_options(SimdFix PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>)

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

function(add_limitless_test name source)
    add_executable(${name} ${source})
    target_compile_options(${name} PRIVATE -O0 -Wall -fprofile-instr-generate -fcoverage-mapping)
    target_link_libraries(${name} PRIVATE SimdFix GTest::gtest GTest::gtest_main)
    target_link_options(${name} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address> -fprofile-instr-generate)
endfunction()

# 2. Use the function to declare your tests
set(BASE_PATH "src/test/cpp/org/limitless/fix/parser")
add_limitless_test(TokenizerTest "${BASE_PATH}/TokenizerTest.cpp")
add_limitless_test(BitSetTest    "${BASE_PATH}/BitSetTest.cpp")
add_limitless_test(ParserTest    "${BASE_PATH}/ParserTest.cpp")
add_limitless_test(PerfectHashTest  "${BASE_PATH}/PerfectHashTest.cpp")
