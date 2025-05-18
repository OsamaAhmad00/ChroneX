#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace googletest = ::testing;

struct OrderbookTestsFixture : public googletest::TestWithParam<const char*>
{
private:
    const static inline std::filesystem::path Root { std::filesystem::current_path() };
    const static inline std::filesystem::path TestFolder { "tests" };
public:
    const static inline std::filesystem::path TestFolderPath { Root / TestFolder };
};

TEST_P(OrderbookTestsFixture, OrderbookTestSuite)
{
    const auto path = TestFolderPath / GetParam();

    std::ifstream file { path };

    std::string line;
    file >> line;

    ASSERT_EQ(line, "test");
    ASSERT_TRUE(file.eof());
}

INSTANTIATE_TEST_CASE_P(
    Tests,
    OrderbookTestsFixture,
    googletest::ValuesIn({
        "test.txt",
    })
);