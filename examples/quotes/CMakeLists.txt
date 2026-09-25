cmake_minimum_required(VERSION 3.20)
project(quotes CXX)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()

# Generate the message classes from this example's spec instead of simdfix's own.
set(SIMDFIX_APP_XML "${CMAKE_CURRENT_SOURCE_DIR}/quotes.xml")
add_subdirectory(../.. simdfix EXCLUDE_FROM_ALL)

add_executable(quotes main.cpp)
target_link_libraries(quotes PRIVATE SimdFix::SimdFix)
add_dependencies(quotes GenerateMessages)
