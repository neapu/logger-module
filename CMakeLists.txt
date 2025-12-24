cmake_minimum_required(VERSION 4.1)

project(Logger VERSION 1.1.0 LANGUAGES CXX)

if (MSVC)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        add_compile_definitions(_AMD64_)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i486|i586|i686|x86)$")
        add_compile_definitions(_X86_)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
        add_compile_definitions(_ARM64_)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM|arm)$")
        add_compile_definitions(_ARM_)
    endif ()
endif ()

add_library(logger)

target_sources(logger
    PUBLIC
        FILE_SET CXX_MODULES FILES
        logger.cppm
)

set_target_properties(logger PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_SCAN_FOR_MODULES ON
)
