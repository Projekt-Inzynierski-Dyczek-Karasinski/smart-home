include_guard(GLOBAL)

# Finalize a target by setting its compile features and linking to the warnings library.
function(_smarthome_finalize target scope folder)
    target_compile_features(${target} ${scope} cxx_std_23)
    if (scope STREQUAL "PUBLIC")
        target_link_libraries(${target} PRIVATE smarthome::warnings)
    endif ()
    if (NOT folder STREQUAL "")
        set_target_properties(${target} PROPERTIES FOLDER "${folder}")
    endif ()
endfunction()


# Create libaray target.
# Header paths seen by consumers are relative to the include root (src directory),
# e.g. #include "common/logger/logger.h"
function(smarthome_add_library name)
    if (NOT DEFINED SMARTHOME_INCLUDE_ROOT)
        message(FATAL_ERROR "smarthome_add_library(${name}): SMARTHOME_INCLUDE_ROOT is not set "
                "for ${CMAKE_CURRENT_SOURCE_DIR}")
    endif ()

    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "FOLDER"
            "SOURCES;HEADERS;PRIVATE_HEADERS;PUBLIC_DEPS;PRIVATE_DEPS")

    if (ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "smarthome_add_library(${name}): unexpected arguments: "
                "${ARG_UNPARSED_ARGUMENTS}")
    endif ()
    if (ARG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "smarthome_add_library(${name}): keywords without values: "
                "${ARG_KEYWORDS_MISSING_VALUES}")
    endif ()
    if (NOT ARG_HEADERS)
        message(FATAL_ERROR "smarthome_add_library(${name}): missing HEADERS")
    endif ()

    set(target smarthome_${name})

    # Add library with sources or header only library as interface
    if (ARG_SOURCES)
        add_library(${target} ${ARG_SOURCES})
        set(scope PUBLIC)
    else ()
        foreach (bad_argument IN ITEMS PRIVATE_DEPS PRIVATE_HEADERS)
            if (ARG_${bad_argument})
                message(FATAL_ERROR "smarthome_add_library(${name}): ${bad_argument} is not allowed "
                        "for a header-only library (no SOURCES given)")
            endif ()
        endforeach ()
        add_library(${target} INTERFACE)
        set(scope INTERFACE)
    endif ()

    add_library(smarthome::${name} ALIAS ${target})

    # Create library targets with public and private headers
    target_sources(${target} ${scope}
            FILE_SET HEADERS
            BASE_DIRS ${SMARTHOME_INCLUDE_ROOT}
            FILES ${ARG_HEADERS}
    )
    if (ARG_PRIVATE_HEADERS)
        target_sources(${target} PRIVATE
                FILE_SET private_headers TYPE HEADERS
                BASE_DIRS ${SMARTHOME_INCLUDE_ROOT}
                FILES ${ARG_PRIVATE_HEADERS}
        )
    endif ()

    # Link dependencies
    if (ARG_PUBLIC_DEPS)
        target_link_libraries(${target} ${scope} ${ARG_PUBLIC_DEPS})
    endif ()
    if (ARG_PRIVATE_DEPS)
        target_link_libraries(${target} PRIVATE ${ARG_PRIVATE_DEPS})
    endif ()

    # Add target to defined or default folder
    if (NOT DEFINED ARG_FOLDER)
        set(ARG_FOLDER "${LIBRARIES_TARGETS_FOLDER}")
    endif ()
    _smarthome_finalize(${target} ${scope} "${ARG_FOLDER}")
endfunction()


# Create executable target.
function(smarthome_add_executable name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "FOLDER" "SOURCES;DEPS")

    if (ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "smarthome_add_executable(${name}): unexpected arguments: "
                "${ARG_UNPARSED_ARGUMENTS}")
    endif ()
    if (ARG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "smarthome_add_executable(${name}): keywords without values: "
                "${ARG_KEYWORDS_MISSING_VALUES}")
    endif ()
    if (NOT ARG_SOURCES)
        message(FATAL_ERROR "smarthome_add_executable(${name}): missing SOURCES")
    endif ()

    # Create executable target and link dependencies
    add_executable(${name} ${ARG_SOURCES})
    if (ARG_DEPS)
        target_link_libraries(${name} PRIVATE ${ARG_DEPS})
    endif ()

    _smarthome_finalize(${name} PUBLIC "${ARG_FOLDER}")
endfunction()