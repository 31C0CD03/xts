cmake_minimum_required(VERSION 3.17)
project(xts)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(xts
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

add_subdirectory(tests)