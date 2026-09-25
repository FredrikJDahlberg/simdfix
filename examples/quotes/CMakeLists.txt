cmake_minimum_required(VERSION 3.20)
project(quotes CXX)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()

# An installed simdfix (-DCMAKE_PREFIX_PATH=<prefix>) is used when there is one;
# otherwise the source tree this example lives in.
find_package(SimdFix CONFIG QUIET)
if(NOT SimdFix_FOUND)
    add_subdirectory(../.. simdfix EXCLUDE_FROM_ALL)
endif()

# Generates QuoteDecoder, QuoteEncoder, ... from this example's spec, in its own namespace.
simdfix_generate(QuoteMessages
        APP_XML quotes.xml
        NAMESPACE quotes::fix)

add_executable(quotes main.cpp)
target_link_libraries(quotes PRIVATE QuoteMessages)
