add_executable(DataStructuresTests LinkedList.cpp)

target_compile_options(DataStructuresTests PRIVATE -Wall -Werror -Wextra -Wpedantic -Wconversion -Wshadow)

target_include_directories(DataStructuresTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(DataStructuresTests PRIVATE
    gtest
    gtest_main
    gmock
)

include(GoogleTest)
gtest_discover_tests(DataStructuresTests)
