# simdfix_generate(<name>
#                  [APP_XML <file>]
#                  [SESSION_XML <file>]
#                  [CONFIG_XML <file>]
#                  [NAMESPACE <ns>]
#                  [OUTPUT_DIR <dir>])
#
# Runs the simdfix Generator over a set of FIX specs and creates an INTERFACE
# library <name> that carries the generated headers and links SimdFix::SimdFix.
# Linking <name> is all a consumer needs: the headers are generated before any
# target that links it is compiled, and regenerated when a spec changes.
#
#   APP_XML      application spec (messages and enums); omit for session messages only
#   SESSION_XML  session spec; defaults to the session.xml shipped with simdfix
#   CONFIG_XML   engine config; defaults to the config.xml shipped with simdfix
#   NAMESPACE    C++ namespace of the generated code; defaults to
#                org::limitless::simdfix::generated. Consumers include
#                <namespace as a path>/messages/FixMessages.hpp. Give each spec its
#                own namespace to use several in one program.
#   OUTPUT_DIR   include root for the generated headers; defaults to
#                ${CMAKE_CURRENT_BINARY_DIR}/<name>
#
# The including file sets SIMDFIX_RESOURCE_DIR to the directory holding the
# default session.xml and config.xml.

if(NOT DEFINED SIMDFIX_RESOURCE_DIR)
    message(FATAL_ERROR "SimdFixGenerate.cmake: SIMDFIX_RESOURCE_DIR is not set")
endif()
set_property(GLOBAL PROPERTY SIMDFIX_RESOURCE_DIR "${SIMDFIX_RESOURCE_DIR}")

function(simdfix_generate name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "APP_XML;SESSION_XML;CONFIG_XML;NAMESPACE;OUTPUT_DIR" "")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "simdfix_generate: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT TARGET SimdFix::Generator)
        message(FATAL_ERROR "simdfix_generate: SimdFix::Generator is not available")
    endif()

    get_property(resourceDir GLOBAL PROPERTY SIMDFIX_RESOURCE_DIR)
    if(NOT ARG_SESSION_XML)
        set(ARG_SESSION_XML "${resourceDir}/session.xml")
    endif()
    if(NOT ARG_CONFIG_XML)
        set(ARG_CONFIG_XML "${resourceDir}/config.xml")
    endif()
    if(NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "org::limitless::simdfix::generated")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    endif()
    foreach(spec ARG_SESSION_XML ARG_CONFIG_XML ARG_APP_XML ARG_OUTPUT_DIR)
        if(${spec})
            get_filename_component(${spec} "${${spec}}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
    endforeach()

    # The generated headers include each other by their namespace as a path (e.g.
    # "org/limitless/simdfix/generated/messages/FixTypes.hpp"), so OUTPUT_DIR is the
    # include root and they are written beneath it.
    string(REPLACE "::" "/" namespacePath "${ARG_NAMESPACE}")
    set(messagesDir "${ARG_OUTPUT_DIR}/${namespacePath}/messages")
    set(configDir "${ARG_OUTPUT_DIR}/${namespacePath}/config")
    set(headers
            ${messagesDir}/FixMessages.hpp ${messagesDir}/FixTypes.hpp
            ${messagesDir}/FixMessageDecoders.hpp ${messagesDir}/FixMessageEncoders.hpp
            ${messagesDir}/FixMessageHandler.hpp ${configDir}/FixEngine.hpp)

    add_custom_command(
            OUTPUT ${headers}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${messagesDir} ${configDir}
            COMMAND SimdFix::Generator --namespace ${ARG_NAMESPACE} ${ARG_SESSION_XML} ${messagesDir} ${ARG_CONFIG_XML} ${configDir} ${ARG_APP_XML}
            DEPENDS SimdFix::Generator ${ARG_SESSION_XML} ${ARG_CONFIG_XML} ${ARG_APP_XML}
            COMMENT "Generating FIX headers for ${name} in ${ARG_NAMESPACE}"
            VERBATIM)

    # Listing the headers as sources makes <name> a build target, so everything
    # linking it waits for generation.
    add_library(${name} INTERFACE ${headers})
    target_include_directories(${name} INTERFACE $<BUILD_INTERFACE:${ARG_OUTPUT_DIR}>)
    target_link_libraries(${name} INTERFACE SimdFix::SimdFix)
endfunction()
