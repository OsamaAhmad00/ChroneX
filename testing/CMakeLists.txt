cmake_minimum_required(VERSION 3.28)

include_directories(
    ${gtest_SOURCE_DIR}/include
    ${gmock_SOURCE_DIR}/include
)

add_subdirectory(googletest)
enable_testing()

add_subdirectory(matching-engine)
add_subdirectory(orderbook)
add_subdirectory(data-structures)
