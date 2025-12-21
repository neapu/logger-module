cmake_minimum_required(VERSION 4.1)

project(Logger VERSION 1.0.0 LANGUAGES CXX)

add_library(logger)

target_sources(logger
    PUBLIC
        FILE_SET CXX_MODULES FILES
        logger.cppm
)

set_target_properties(logger PROPERTIES
    CXX_STANDARD 23
)