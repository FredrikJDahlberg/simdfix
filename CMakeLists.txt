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
        ${GENERATED_DIR}/FixTypes.hpp ${GENERATED_DIR}/FixMessageDecoders.hpp
        ${GENERATED_DIR}/FixMessageEncoders.hpp ${GENERATED_DIR}/FixMessageHandler.hpp)
set(FIX_HEADERS
        ${MAIN_PATH}/utils/Utils.hpp ${MAIN_PATH}/utils/BitSet64.hpp
        ${MAIN_PATH}/decoder/PayloadDecoder.hpp ${MAIN_PATH}/decoder/MessageDecoder.hpp
        ${MAIN_PATH}/CodecTypes.hpp ${MAIN_PATH}/decoder/FieldDecoder.hpp
        ${MAIN_PATH}/decoder/GroupDecoder.hpp ${MAIN_PATH}/simd/Uint8x16.hpp
        ${MAIN_PATH}/simd/LinearSearch.hpp ${MAIN_PATH}/encoder/FieldEncoder.hpp
        ${MAIN_PATH}/encoder/PayloadEncoder.hpp ${MAIN_PATH}/encoder/MessageEncoder.hpp
        ${MAIN_PATH}/encoder/GroupEncoder.hpp ${MAIN_PATH}/encoder/GroupEncoder.hpp)
add_library(SimdFix ${FIX_HEADERS} ${GENERATED_HEADERS})
target_sources(SimdFix INTERFACE ${FIX_HEADERS} ${GENERATED_HEADERS})
target_include_directories(SimdFix INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")
target_compile_definitions(SimdFix INTERFACE $<$<CONFIG:Debug>:DEBUG_MODE> $<$<CONFIG:Release>:NDEBUG>
)
target_compile_options(SimdFix INTERFACE
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer -fprofile-instr-generate -fcoverage-mapping>
        $<$<CONFIG:Release>:-O3 -march=native>
)
target_link_options(SimdFix INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>
)

add_executable(SimdFixBenchmark ${TEST_PATH}/benchmark/SimdFixBenchmark.cpp)
target_include_directories(SimdFixBenchmark PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/test/cpp")
target_link_libraries(SimdFixBenchmark PRIVATE SimdFix)
add_dependencies(SimdFixBenchmark GenerateMessages)
target_compile_definitions(SimdFixBenchmark PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(SimdFixBenchmark PRIVATE -Wall
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O3 -march=native>
)
target_link_options(SimdFixBenchmark PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>)
set_target_properties(SimdFixBenchmark PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

add_custom_target(ProfileCounters
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/counterprofile.trace
        COMMAND xcrun xctrace record --template "CPU Counters" --output ${CMAKE_BINARY_DIR}/counterprofile.trace --launch -- $<TARGET_FILE:SimdFixBenchmark> all
        DEPENDS SimdFixBenchmark
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running SimdFix benchmarks under Instruments CPU Counters (use a Release build for meaningful numbers)"
        USES_TERMINAL
)

add_custom_target(ProfileTime
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/timeprofile.trace
        COMMAND xcrun xctrace record --template "Time Profiler" --output ${CMAKE_BINARY_DIR}/timeprofile.trace --launch -- $<TARGET_FILE:SimdFixBenchmark> all
        DEPENDS SimdFixBenchmark
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running SimdFix benchmarks under Instruments Time Profiler (use a Release build for meaningful numbers)"
        USES_TERMINAL
)

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
                ${GENERATOR_PATH}/FixGenerator.cpp ${GENERATOR_PATH}/DataModel.hpp
        COMMENT "Generating FIX message headers from protocol.xml"
        VERBATIM
)
add_custom_target(GenerateMessages DEPENDS ${GENERATED_HEADERS})
add_dependencies(SimdFix GenerateMessages)

include(GoogleTest)

function(add_fix_test name source)
    add_executable(${name} ${source})
    target_compile_options(${name} PRIVATE -O0 -Wall -fprofile-instr-generate -fcoverage-mapping)
    target_link_libraries(${name} PRIVATE SimdFix GTest::gtest_main)
    target_link_options(${name} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address> -fprofile-instr-generate)
    add_dependencies(${name} GenerateMessages)
    gtest_discover_tests(${name})
endfunction()

add_fix_test(PayloadDecoderTest     "${TEST_PATH}/decoder/PayloadDecoderTest.cpp")
add_fix_test(MessageDecoderTest      "${TEST_PATH}/decoder/MessageDecoderTest.cpp")
add_fix_test(MessageEncoderTest      "${TEST_PATH}/encoder/MessageEncoderTest.cpp")
add_fix_test(GroupDecoderTest "${TEST_PATH}/decoder/GroupDecoderTest.cpp")
add_fix_test(FieldDecoderTest "${TEST_PATH}/decoder/FieldDecoderTest.cpp")
add_fix_test(UtilsTest       "${TEST_PATH}/utils/UtilsTest.cpp")
add_fix_test(BitSetTest      "${TEST_PATH}/utils/BitSetTest.cpp")
add_fix_test(Uint8x16Test    "${TEST_PATH}/simd/Uint8x16Test.cpp")

set(FIX_TEST_TARGETS
        PayloadDecoderTest
        MessageDecoderTest MessageEncoderTest
        GroupDecoderTest
        UtilsTest BitSetTest Uint8x16Test)

add_custom_target(AllTests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS ${FIX_TEST_TARGETS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building and running all test suites"
        USES_TERMINAL
)

# Dynamically create an executable or library treating headers as source items
# Using set_source_files_properties forces CMake/Compilers to attempt compilation
add_library(header_test EXCLUDE_FROM_ALL ${FIX_HEADERS} ${FIX_GENERATED_HEADERS})
set_source_files_properties(${FIX_HEADERS} PROPERTIES LANGUAGE CXX)