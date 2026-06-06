cmake_minimum_required(VERSION 3.20)
project(simdfix)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp
        ${CMAKE_SOURCE_DIR}/src/test/cpp
        ${CMAKE_SOURCE_DIR}/src/generator)
set(MAIN_PATH "src/main/cpp/org/limitless/fix")
set(GEN_PATH "src/main/cpp/org/limitless/fix/messages")
set(TEST_PATH "src/test/cpp/org/limitless/fix")
set(GENERATOR_PATH "src/generator/org/limitless/generator/fix/")
set(PROTOCOL_XML "${CMAKE_SOURCE_DIR}/src/generator/resources/protocol.xml")
set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/${GEN_PATH}")
set(GENERATED_HEADERS
        ${GENERATED_DIR}/Grammar.hpp
        ${GENERATED_DIR}/MessageDecoders.hpp
        ${GENERATED_DIR}/MessageHandler.hpp)
set(FIX_HEADERS
        ${MAIN_PATH}/utils/Utils.hpp
        ${MAIN_PATH}/utils/BitSet64.hpp
        ${MAIN_PATH}/decoder/Decoder.hpp
        ${MAIN_PATH}/decoder/MessageDecoder.hpp
        ${MAIN_PATH}/decoder/Result.hpp
        ${MAIN_PATH}/decoder/Dictionary.hpp
        ${MAIN_PATH}/decoder/GroupDecoder.hpp
        ${MAIN_PATH}/decoder/Token.hpp
        ${MAIN_PATH}/simd/Uint8x16.hpp
        ${MAIN_PATH}/simd/LinearSearch.hpp)
add_library(SimdFix INTERFACE)
target_sources(SimdFix INTERFACE ${FIX_HEADERS} ${GENERATED_HEADERS})
target_include_directories(SimdFix INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")
target_compile_definitions(SimdFix INTERFACE
        $<$<CONFIG:Debug>:DEBUG_MODE>
        $<$<CONFIG:Release>:NDEBUG>
)
target_compile_options(SimdFix INTERFACE
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer -fprofile-instr-generate -fcoverage-mapping>
        $<$<CONFIG:Release>:-O3 -march=native>
)
target_link_options(SimdFix INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>
)

add_executable(SimdFixBench ${TEST_PATH}/benchmark/SimdFixBench.cpp)
target_include_directories(SimdFixBench PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/test/cpp")
target_link_libraries(SimdFixBench PRIVATE SimdFix)
add_dependencies(SimdFixBench GenerateMessages)
target_compile_definitions(SimdFixBench PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(SimdFixBench PRIVATE -Wall
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O3 -march=native>
)
target_link_options(SimdFixBench PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>)
set_target_properties(SimdFixBench PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/include")
find_package(pugixml REQUIRED)
add_executable(Generator ${GENERATOR_PATH}/FixGenerator.cpp)
target_compile_options(Generator PRIVATE -O0 -Wall)
target_include_directories(Generator PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/generator/cpp")
target_link_libraries(Generator PRIVATE pugixml::pugixml)

add_custom_command(
        OUTPUT ${GENERATED_HEADERS}
        COMMAND $<TARGET_FILE:Generator> ${PROTOCOL_XML} ${GENERATED_DIR}
        DEPENDS Generator
                ${PROTOCOL_XML}
                ${GENERATOR_PATH}/FixGenerator.cpp ${GENERATOR_PATH}/Generator.hpp ${GENERATOR_PATH}/Processor.hpp
        COMMENT "Generating FIX message headers from protocol.xml"
        VERBATIM
)
add_custom_target(GenerateMessages DEPENDS ${GENERATED_HEADERS})
add_dependencies(SimdFix GenerateMessages)

function(add_fix_test name source)
    add_executable(${name} ${source})
    target_compile_options(${name} PRIVATE -O0 -Wall -fprofile-instr-generate -fcoverage-mapping)
    target_link_libraries(${name} PRIVATE SimdFix GTest::gtest GTest::gtest_main)
    target_link_options(${name} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address> -fprofile-instr-generate)
    add_dependencies(${name} GenerateMessages)
endfunction()

add_fix_test(DecoderTest     "${TEST_PATH}/parser/DecoderTest.cpp")
add_fix_test(ParserTest      "${TEST_PATH}/parser/ParserTest.cpp")
add_fix_test(UtilsTest       "${TEST_PATH}/utils/UtilsTest.cpp")
add_fix_test(BitSetTest      "${TEST_PATH}/utils/BitSetTest.cpp")
add_fix_test(Uint8x16Test    "${TEST_PATH}/simd/Uint8x16Test.cpp")
