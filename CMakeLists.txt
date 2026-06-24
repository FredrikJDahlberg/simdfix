cmake_minimum_required(VERSION 3.20)
project(simdfix)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

find_package(GTest REQUIRED)

# The x86 SSE backend (Uint8x16_sse.inl) uses _mm_blendv_epi8, an SSE4.1
# intrinsic. The Release build already enables it via -march=native, but the
# Debug build sets no architecture flags, so on x86 it fails to compile without
# this. check_cxx_compiler_flag adds -msse4.1 only where the compiler accepts
# it (x86 targets); on arm64 the flag is rejected and SIMDFIX_SSE41_FLAG stays
# empty, since NEON is baseline there.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-msse4.1" SIMDFIX_HAS_MSSE41)
set(SIMDFIX_SSE41_FLAG "")
if(SIMDFIX_HAS_MSSE41)
    set(SIMDFIX_SSE41_FLAG -msse4.1)
endif()

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
        ${MAIN_PATH}/Fix.hpp ${MAIN_PATH}/Types.hpp ${MAIN_PATH}/detail/Tokens.hpp
        ${MAIN_PATH}/utils/Conversions.hpp ${MAIN_PATH}/utils/FixedDecimal.hpp ${MAIN_PATH}/utils/OptionalInt.hpp
        ${MAIN_PATH}/decoder/PayloadDecoder.hpp ${MAIN_PATH}/decoder/MessageDecoder.hpp
        ${MAIN_PATH}/detail/decoder/FieldDecoder.hpp
        ${MAIN_PATH}/decoder/DataDecoder.hpp ${MAIN_PATH}/decoder/GroupDecoder.hpp
        ${MAIN_PATH}/detail/Expected.hpp ${MAIN_PATH}/detail/ByteSwap.hpp
        ${MAIN_PATH}/detail/simd/Uint8x16.hpp
        ${MAIN_PATH}/detail/simd/LinearSearch.hpp ${MAIN_PATH}/detail/encoder/FieldEncoder.hpp
        ${MAIN_PATH}/encoder/DataEncoder.hpp ${MAIN_PATH}/encoder/PayloadEncoder.hpp
        ${MAIN_PATH}/encoder/MessageEncoder.hpp ${MAIN_PATH}/encoder/GroupEncoder.hpp
        src/main/cpp/org/limitless/fix/session/Session.hpp)
add_library(SimdFix INTERFACE)
add_library(SimdFix::SimdFix ALIAS SimdFix)
# In-tree builds (tests, benchmarks) see the headers where they live; installed
# consumers see them under <prefix>/include. The public entry point is
# org/limitless/fix/Fix.hpp; anything under org/limitless/fix/detail/ is internal.
target_include_directories(SimdFix INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp>
        $<INSTALL_INTERFACE:include>)
target_compile_definitions(SimdFix INTERFACE $<$<CONFIG:Debug>:DEBUG_MODE> $<$<CONFIG:Release>:NDEBUG>
)
target_compile_options(SimdFix INTERFACE
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer -fprofile-instr-generate -fcoverage-mapping ${SIMDFIX_SSE41_FLAG}>
        $<$<CONFIG:Release>:-O3 -march=native>
)
target_link_options(SimdFix INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address -fprofile-instr-generate>
)

add_library(SimdFixSession STATIC
        src/main/cpp/org/limitless/fix/session/Session.cpp
        src/main/cpp/org/limitless/fix/session/Session.hpp)
target_link_libraries(SimdFixSession PUBLIC SimdFix)
add_dependencies(SimdFixSession GenerateMessages)

# --- Install / packaging --------------------------------------------------
# Makes the header-only library consumable via find_package(SimdFix). Public
# headers install under <prefix>/include preserving the org/limitless/fix tree.
# detail/ is excluded: all public API signatures use opaque types; the detail
# headers are only needed for in-tree builds (tests, benchmarks).
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS SimdFix EXPORT SimdFixTargets)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/org
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "*.inl"
            PATTERN "detail" EXCLUDE
)

install(EXPORT SimdFixTargets
        NAMESPACE SimdFix::
        FILE SimdFixTargets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SimdFix)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SimdFixConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/SimdFixConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SimdFix)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SimdFixConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SimdFix)
# --------------------------------------------------------------------------

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

# Standalone micro-benchmark for the timestamp parsing path (header-only; no
# generated messages needed). Build a Release configuration for real numbers.
add_executable(TimestampBenchmark ${TEST_PATH}/benchmark/TimestampBenchmark.cpp)
target_include_directories(TimestampBenchmark PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp")
target_link_libraries(TimestampBenchmark PRIVATE SimdFix)
target_compile_definitions(TimestampBenchmark PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(TimestampBenchmark PRIVATE -Wall
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address -fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O3 -march=native>
)
target_link_options(TimestampBenchmark PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
set_target_properties(TimestampBenchmark PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

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
set_target_properties(Generator PROPERTIES CXX_STANDARD 23)
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
# SimdFix is an INTERFACE (header-only) target and does not build, so it needs
# no dependency on GenerateMessages; every consumer depends on it explicitly.

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
add_fix_test(PayloadDecoderFuzzTest "${TEST_PATH}/decoder/PayloadDecoderFuzzTest.cpp")
# Fuzz the production code path: strip the decoder's debug diagnostics (which
# would otherwise dump on every one of the tens of thousands of iterations).
# ASan is independent of NDEBUG and stays active; the bounded debug-print paths
# are covered by PayloadDecoder.TruncationSafety in the Debug build.
target_compile_definitions(PayloadDecoderFuzzTest PRIVATE NDEBUG)
add_fix_test(MessageDecoderTest      "${TEST_PATH}/decoder/MessageDecoderTest.cpp")
add_fix_test(MessageEncoderTest      "${TEST_PATH}/encoder/MessageEncoderTest.cpp")
add_fix_test(GroupDecoderTest "${TEST_PATH}/decoder/GroupDecoderTest.cpp")
add_fix_test(FieldDecoderTest "${TEST_PATH}/decoder/FieldDecoderTest.cpp")
add_fix_test(ConversionsTest       "${TEST_PATH}/utils/ConversionsTest.cpp")
add_fix_test(FixedDecimalTest "${TEST_PATH}/utils/FixedDecimalTest.cpp")
add_fix_test(OptionalIntTest  "${TEST_PATH}/utils/OptionalIntTest.cpp")
add_fix_test(Uint8x16Test       "${TEST_PATH}/simd/Uint8x16Test.cpp")
add_fix_test(LinearSearchTest  "${TEST_PATH}/simd/LinearSearchTest.cpp")

set(FIX_TEST_TARGETS
        PayloadDecoderTest PayloadDecoderFuzzTest
        MessageDecoderTest MessageEncoderTest
        GroupDecoderTest FieldDecoderTest
        ConversionsTest FixedDecimalTest OptionalIntTest Uint8x16Test LinearSearchTest)

add_custom_target(AllTests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS ${FIX_TEST_TARGETS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building and running all test suites"
        USES_TERMINAL
)

find_program(XCRUN xcrun)
if(XCRUN)
    set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)

    # Build per-binary run commands and profraw path list
    set(_PROFRAW_FILES "")
    set(_RUN_COMMANDS "")
    foreach(_tgt IN LISTS FIX_TEST_TARGETS)
        list(APPEND _PROFRAW_FILES ${COVERAGE_DIR}/${_tgt}.profraw)
    endforeach()

    # First target is the primary binary for llvm-cov; the rest use -object
    list(GET FIX_TEST_TARGETS 0 _PRIMARY)
    set(_COV_OBJECTS "")
    foreach(_tgt IN LISTS FIX_TEST_TARGETS)
        if(NOT _tgt STREQUAL _PRIMARY)
            list(APPEND _COV_OBJECTS -object $<TARGET_FILE:${_tgt}>)
        endif()
    endforeach()

    set(_IGNORE_REGEX "--ignore-filename-regex=(test|gtest|GoogleTest|benchmark)")
    add_custom_target(Coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/PayloadDecoderTest.profraw"   $<TARGET_FILE:PayloadDecoderTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/PayloadDecoderFuzzTest.profraw" $<TARGET_FILE:PayloadDecoderFuzzTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/MessageDecoderTest.profraw"   $<TARGET_FILE:MessageDecoderTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/MessageEncoderTest.profraw"   $<TARGET_FILE:MessageEncoderTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/GroupDecoderTest.profraw"     $<TARGET_FILE:GroupDecoderTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/FieldDecoderTest.profraw"     $<TARGET_FILE:FieldDecoderTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/ConversionsTest.profraw"            $<TARGET_FILE:ConversionsTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/FixedDecimalTest.profraw"     $<TARGET_FILE:FixedDecimalTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/OptionalIntTest.profraw"      $<TARGET_FILE:OptionalIntTest>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/Uint8x16Test.profraw"         $<TARGET_FILE:Uint8x16Test>
        COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${COVERAGE_DIR}/LinearSearchTest.profraw"    $<TARGET_FILE:LinearSearchTest>
        COMMAND ${XCRUN} llvm-profdata merge -sparse ${_PROFRAW_FILES} -o ${COVERAGE_DIR}/coverage.profdata
        COMMAND ${XCRUN} llvm-cov report
            $<TARGET_FILE:${_PRIMARY}>
            ${_COV_OBJECTS}
            -instr-profile=${COVERAGE_DIR}/coverage.profdata
            ${_IGNORE_REGEX}
        DEPENDS ${FIX_TEST_TARGETS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Collecting coverage from all test suites"
        USES_TERMINAL
        VERBATIM
    )
endif()

# Dynamically create an executable or library treating headers as source items
# Using set_source_files_properties forces CMake/Compilers to attempt compilation
add_library(header_test ${FIX_HEADERS} ${GENERATED_HEADERS}
        src/main/cpp/org/limitless/fix/session/Session.hpp)
set_source_files_properties(${FIX_HEADERS} ${GENERATED_HEADERS} PROPERTIES LANGUAGE CXX)
target_include_directories(header_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
add_dependencies(header_test GenerateMessages)