cmake_minimum_required(VERSION 3.20)
project(simd_fix)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp ${CMAKE_SOURCE_DIR}/src/test/cpp)
set(MAIN_PATH "src/main/cpp/org/limitless/fix/parser")
set(TEST_PATH "src/test/cpp/org/limitless/fix/parser")
set(GEN_PATH "src/main/cpp/org/limitless/fix/messages")
set(GENERATED_HEADERS
        ${GEN_PATH}/Grammar.hpp
        ${GEN_PATH}/LogonDecoder.hpp
        ${GEN_PATH}/LogoutDecoder.hpp
        ${GEN_PATH}/MessageHandler.hpp)
set(FIX_SOURCES
        ${MAIN_PATH}/Decoder.cpp
        ${MAIN_PATH}/Tokenizer.cpp)
set(FIX_HEADERS
        ${MAIN_PATH}/Tokenizer.hpp
        ${MAIN_PATH}/Utils.hpp
        ${MAIN_PATH}/Decoder.hpp
        ${MAIN_PATH}/BitSet64.hpp
        ${MAIN_PATH}/MessageDecoder.hpp
        ${MAIN_PATH}/ParserStatus.hpp
        ${MAIN_PATH}/Dictionary.hpp
        ${MAIN_PATH}/GroupDecoder.hpp
        ${MAIN_PATH}/PerfectHashMap.hpp
        src/main/cpp/org/limitless/fix/messages/Grammar.hpp
        src/main/cpp/org/limitless/fix/parser/Token.hpp
        src/main/cpp/org/limitless/fix/messages/HopGroupDecoder.hpp
        src/main/cpp/org/limitless/fix/messages/HeaderDecoder.hpp)

add_library(SimdFix STATIC ${FIX_SOURCES} ${FIX_HEADERS} ${GENERATED_HEADERS})
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

function(add_test name source)
    add_executable(${name} ${source})
    target_compile_options(${name} PRIVATE -O0 -Wall -fprofile-instr-generate -fcoverage-mapping)
    target_link_libraries(${name} PRIVATE SimdFix GTest::gtest GTest::gtest_main)
    target_link_options(${name} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address> -fprofile-instr-generate)
endfunction()

add_test(TokenizerTest   "${TEST_PATH}/TokenizerTest.cpp")
add_test(BitSetTest      "${TEST_PATH}/BitSetTest.cpp")
add_test(ParserTest      "${TEST_PATH}/ParserTest.cpp")
add_test(PerfectHashTest "${TEST_PATH}/PerfectHashMapTest.cpp")
