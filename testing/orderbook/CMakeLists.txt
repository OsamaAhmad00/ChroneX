add_executable(OrderbookTests Tests.cpp)

target_compile_options(OrderbookTests PRIVATE -Wall -Werror -Wextra -Wpedantic -Wconversion -Wshadow)

target_include_directories(OrderbookTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(OrderbookTests PRIVATE
    gtest
    gtest_main
    gmock
)

# Copy the tests folder to the test build directory
add_custom_target(
    CopyTestFiles ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_BINARY_DIR}/tests
)

add_dependencies(OrderbookTests CopyTestFiles)

include(GoogleTest)
gtest_discover_tests(OrderbookTests)
