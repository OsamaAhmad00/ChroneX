add_executable(MatchingEngineTests Tests.cpp ${CHRONEX_SOURCES})

target_compile_options(MatchingEngineTests PRIVATE -Wall -Werror -Wextra -Wpedantic -Wconversion -Wshadow)

target_include_directories(MatchingEngineTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(MatchingEngineTests PRIVATE
    gtest
    gtest_main
    gmock
)

include(GoogleTest)
gtest_discover_tests(MatchingEngineTests)
