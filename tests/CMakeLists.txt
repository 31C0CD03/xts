cmake_minimum_required(VERSION 3.17)

include(CTest)
enable_testing()

add_executable(test_xts
    test_algorithm.cpp
)

target_link_libraries(test_xts
    PRIVATE
        xts
)

add_test(
    NAME xts_tests
    COMMAND test_xts
)