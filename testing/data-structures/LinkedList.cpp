#include <list>
#include <ranges>

#include <gtest/gtest.h>

#include <chronex/data-structures/LinkedList.hpp>

using namespace chronex::ds;

class LinkedListTest : public testing::Test {
protected:
    LinkedList<int> list;
};

TEST_F(LinkedListTest, DefaultConstructor) {
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
    EXPECT_EQ(list.begin(), list.end());
}

TEST_F(LinkedListTest, SizeConstructor) {
    LinkedList<int> l(3, 42);
    EXPECT_EQ(l.size(), 3);
    EXPECT_FALSE(l.empty());
    auto it = l.begin();
    EXPECT_EQ(*it, 42);
    ++it;
    EXPECT_EQ(*it, 42);
    ++it;
    EXPECT_EQ(*it, 42);
}

TEST_F(LinkedListTest, CopyConstructor) {
    list.push_back(1);
    list.push_back(2);
    LinkedList<int> copy(list);
    EXPECT_EQ(copy.size(), 2);
    auto it = copy.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
}

TEST_F(LinkedListTest, MoveConstructor) {
    list.push_back(1);
    list.push_back(2);
    LinkedList<int> moved(std::move(list));
    EXPECT_EQ(moved.size(), 2);
    EXPECT_TRUE(list.empty());
    auto it = moved.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
}

TEST_F(LinkedListTest, CopyAssignment) {
    list.push_back(1);
    LinkedList<int> other;
    other = list;
    EXPECT_EQ(other.size(), 1);
    EXPECT_EQ(*other.begin(), 1);
}

TEST_F(LinkedListTest, MoveAssignment) {
    list.push_back(1);
    LinkedList<int> other;
    other = std::move(list);
    EXPECT_EQ(other.size(), 1);
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(*other.begin(), 1);
}

TEST_F(LinkedListTest, PushFront) {
    list.push_front(1);
    list.push_front(2);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list.front(), 2);
    EXPECT_EQ(list.back(), 1);
}

TEST_F(LinkedListTest, PushBack) {
    list.push_back(1);
    list.push_back(2);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.back(), 2);
}

TEST_F(LinkedListTest, PopFront) {
    list.push_back(1);
    list.push_back(2);
    list.pop_front();
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list.front(), 2);
}

TEST_F(LinkedListTest, PopBack) {
    list.push_back(1);
    list.push_back(2);
    list.pop_back();
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list.back(), 1);
}

TEST_F(LinkedListTest, Insert) {
    list.push_back(1);
    list.push_back(3);
    auto it = list.insert(std::next(list.begin()), 2);
    EXPECT_EQ(list.size(), 3);
    EXPECT_EQ(*it, 2);
    auto check = list.begin();
    EXPECT_EQ(*check, 1);
    ++check;
    EXPECT_EQ(*check, 2);
    ++check;
    EXPECT_EQ(*check, 3);
}

TEST_F(LinkedListTest, Erase) {
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);
    auto it = list.erase(std::next(list.begin()));
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.back(), 3);
}

TEST_F(LinkedListTest, EraseRange) {
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);
    list.push_back(4);
    auto it = list.erase(std::next(list.begin()), std::prev(list.end()));
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.back(), 4);
}

TEST_F(LinkedListTest, Extract) {
    list.push_back(1);
    list.push_back(2);
    auto it = list.extract(std::next(list.begin()));
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(*it, 2);
}

TEST_F(LinkedListTest, ExtractRange) {
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);
    auto extracted = list.extract_range(std::next(list.begin()), list.end());
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(extracted.size(), 2);
    EXPECT_EQ(list.front(), 1);
    auto it = extracted.begin();
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(*it, 3);
}

TEST_F(LinkedListTest, Splice) {
    LinkedList<int> other;
    other.push_back(2);
    other.push_back(3);
    list.push_back(1);
    list.push_back(4);
    list.splice(std::next(list.begin()), other, other.begin(), other.end());
    EXPECT_EQ(list.size(), 4);
    EXPECT_TRUE(other.empty());
    auto it = list.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(*it, 3);
    ++it;
    EXPECT_EQ(*it, 4);
}

TEST_F(LinkedListTest, Clear) {
    list.push_back(1);
    list.push_back(2);
    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
}

TEST_F(LinkedListTest, Iterator) {
    list.push_back(1);
    list.push_back(2);
    auto it = list.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    --it;
    EXPECT_EQ(*it, 1);
}

TEST_F(LinkedListTest, ReverseIterator) {
    list.push_back(1);
    list.push_back(2);
    auto rit = list.rbegin();
    EXPECT_EQ(*rit, 2);
    ++rit;
    EXPECT_EQ(*rit, 1);
}

TEST_F(LinkedListTest, ConstIterator) {
    list.push_back(1);
    list.push_back(2);
    const LinkedList<int>& const_list = list;
    auto cit = const_list.cbegin();
    EXPECT_EQ(*cit, 1);
    ++cit;
    EXPECT_EQ(*cit, 2);
}

// Custom class to test with non-trivial objects
class TestObject {
public:
    explicit TestObject(int value = 0, std::string name = "")
        : value_(value), name_(std::move(name)), destroyed_(false) {}
    
    TestObject(const TestObject& other) = default;
    
    TestObject(TestObject&& other) noexcept
        : value_(other.value_), name_(std::move(other.name_)), destroyed_(other.destroyed_) {
        other.value_ = 0;
        other.destroyed_ = true;
    }
    
    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            value_ = other.value_;
            name_ = other.name_;
            destroyed_ = other.destroyed_;
        }
        return *this;
    }
    
    TestObject& operator=(TestObject&& other) noexcept {
        if (this != &other) {
            value_ = other.value_;
            name_ = std::move(other.name_);
            destroyed_ = other.destroyed_;
            other.value_ = 0;
            other.destroyed_ = true;
        }
        return *this;
    }
    
    ~TestObject() {
        destroyed_ = true;
    }
    
    bool operator==(const TestObject& other) const {
        return value_ == other.value_ && name_ == other.name_;
    }

    [[nodiscard]] int get_value() const { return value_; }
    [[nodiscard]] const std::string& get_name() const { return name_; }
    [[nodiscard]] bool is_destroyed() const { return destroyed_; }
    
private:

    int value_;
    std::string name_;
    bool destroyed_;
};

class ComplexLinkedListTest : public testing::Test {
protected:
    LinkedList<TestObject> objectList;
    LinkedList<std::unique_ptr<int>> uniquePtrList;
    LinkedList<std::shared_ptr<int>> sharedPtrList;
    LinkedList<std::vector<int>> nestedList;
};

TEST_F(ComplexLinkedListTest, CustomObjectOperations) {
    // Test with custom objects
    objectList.push_back(TestObject(1, "one"));
    objectList.push_back(TestObject(2, "two"));
    objectList.push_back(TestObject(3, "three"));
    
    EXPECT_EQ(objectList.size(), 3);
    EXPECT_EQ(objectList.front().get_value(), 1);
    EXPECT_EQ(objectList.front().get_name(), "one");
    EXPECT_EQ(objectList.back().get_value(), 3);
    EXPECT_EQ(objectList.back().get_name(), "three");
    
    // Test copy of complex objects
    LinkedList<TestObject> copiedList(objectList);
    EXPECT_EQ(copiedList.size(), 3);
    
    // Test move of complex objects
    LinkedList<TestObject> movedList(std::move(copiedList));
    EXPECT_EQ(movedList.size(), 3);
    EXPECT_TRUE(copiedList.empty());
    
    // Test erase with complex objects
    auto it = std::next(movedList.begin());
    auto nextIt = movedList.erase(it);
    EXPECT_EQ(movedList.size(), 2);
    EXPECT_EQ(nextIt->get_value(), 3);
}

TEST_F(ComplexLinkedListTest, UniquePointerStorage) {
    // Test with unique_ptr (ownership transfer)
    uniquePtrList.push_back(std::make_unique<int>(10));
    uniquePtrList.push_back(std::make_unique<int>(20));
    
    EXPECT_EQ(uniquePtrList.size(), 2);
    EXPECT_EQ(*uniquePtrList.front(), 10);
    EXPECT_EQ(*uniquePtrList.back(), 20);
    
    // Test move operations with unique_ptr
    auto extractedPtr = uniquePtrList.extract(uniquePtrList.begin());
    EXPECT_EQ(uniquePtrList.size(), 1);
    EXPECT_EQ(**extractedPtr, 10);
    
    // Test splice operations with unique_ptr
    LinkedList<std::unique_ptr<int>> otherList;
    otherList.push_back(std::make_unique<int>(30));
    otherList.push_back(std::make_unique<int>(40));
    
    uniquePtrList.splice(uniquePtrList.end(), otherList, otherList.begin(), otherList.end());
    EXPECT_EQ(uniquePtrList.size(), 3);
    EXPECT_TRUE(otherList.empty());
    
    // Check values after splice
    auto valueIt = uniquePtrList.begin();
    EXPECT_EQ(**valueIt, 20);
    ++valueIt;
    EXPECT_EQ(**valueIt, 30);
    ++valueIt;
    EXPECT_EQ(**valueIt, 40);
}

TEST_F(ComplexLinkedListTest, SharedPointerStorage) {
    // Test with shared_ptr (reference counting)
    auto ptr1 = std::make_shared<int>(100);
    auto ptr2 = std::make_shared<int>(200);
    
    sharedPtrList.push_back(ptr1);
    sharedPtrList.push_back(ptr2);
    sharedPtrList.push_back(ptr1); // Same pointer added twice
    
    EXPECT_EQ(sharedPtrList.size(), 3);
    EXPECT_EQ(ptr1.use_count(), 3); // Original + 2 in a list
    
    // Test removing shared_ptr
    sharedPtrList.pop_front();
    EXPECT_EQ(ptr1.use_count(), 2); // Original + 1 in a list
    
    // Test clearing
    sharedPtrList.clear();
    EXPECT_EQ(ptr1.use_count(), 1); // Only original left
    EXPECT_EQ(ptr2.use_count(), 1);
}

TEST_F(ComplexLinkedListTest, NestedContainers) {
    // Test with nested containers
    nestedList.push_back(std::vector<int>{1, 2, 3});
    nestedList.push_back(std::vector<int>{4, 5, 6});
    
    EXPECT_EQ(nestedList.size(), 2);
    EXPECT_EQ(nestedList.front().size(), 3);
    EXPECT_EQ(nestedList.front()[0], 1);
    
    // Test modifying nested containers
    nestedList.front().push_back(10);
    EXPECT_EQ(nestedList.front().size(), 4);
    EXPECT_EQ(nestedList.front()[3], 10);
    
    // Test copy of nested containers
    LinkedList<std::vector<int>> copiedNestedList(nestedList);
    EXPECT_EQ(copiedNestedList.size(), 2);
    EXPECT_EQ(copiedNestedList.front().size(), 4);
    
    // Modify original and verify copy is independent
    nestedList.front()[0] = 100;
    EXPECT_EQ(nestedList.front()[0], 100);
    EXPECT_EQ(copiedNestedList.front()[0], 1);
}

TEST_F(LinkedListTest, STLAlgorithmIntegration) {
    // Prepare test data
    list.clear();
    for (int i = 1; i <= 10; ++i) {
        list.push_back(i);
    }

    // Test std::find
    auto it = std::find(list.begin(), list.end(), 5);
    EXPECT_NE(it, list.end());
    EXPECT_EQ(*it, 5);

    it = std::find(list.begin(), list.end(), 11);
    EXPECT_EQ(it, list.end());

    // Test std::count
    auto count = std::count(list.begin(), list.end(), 5);
    EXPECT_EQ(count, 1);

    // Test std::remove_if with erase
    auto removeIt = std::remove_if(list.begin(), list.end(),
                                  [](int val) { return val % 2 == 0; });
    list.erase(removeIt, list.end());
    EXPECT_EQ(list.size(), 5); // Only odd numbers remain

    // Test std::transform
    LinkedList<int> result;
    std::transform(list.begin(), list.end(), std::back_inserter(result),
                  [](int val) { return val * 2; });
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(*result.begin(), 2);

    // Test std::sort - create a vector from the list to sort
    std::vector<int> sortedVec(list.begin(), list.end());
    std::ranges::sort(sortedVec, std::greater<>());

    // Reconstruct the list from the sorted vector
    list.clear();
    for (int val : sortedVec) {
        list.push_back(val);
    }

    // Verify the list is sorted in descending order
    it = list.begin();
    int prev = *it;
    ++it;
    while (it != list.end()) {
        EXPECT_GT(prev, *it);
        prev = *it;
        ++it;
    }
}

TEST_F(LinkedListTest, EdgeCases) {
    // Test with empty lists
    LinkedList<int> emptyList;
    LinkedList<int> anotherEmptyList;

    // Splice empty lists
    emptyList.splice(emptyList.begin(), anotherEmptyList,
                     anotherEmptyList.begin(), anotherEmptyList.end());
    EXPECT_TRUE(emptyList.empty());

    // Test with extremely large lists
    constexpr size_t largeSize = 10000;
    LinkedList<int> largeList(largeSize, 1);
    EXPECT_EQ(largeList.size(), largeSize);

    // Test iterating through extremely large lists
    size_t count = 0;
    for (auto val : largeList) {
        EXPECT_EQ(val, 1);
        ++count;
    }
    EXPECT_EQ(count, largeSize);

    // Test clear on a large list
    largeList.clear();
    EXPECT_TRUE(largeList.empty());

    // Test with recursive structures
    struct Node {
        int value;
        LinkedList<Node*> children;

        explicit Node(int v) : value(v) {}
    };

    LinkedList<Node> nodeList;
    nodeList.push_back(Node(1));
    nodeList.front().children.push_back(new Node(2));
    nodeList.front().children.push_back(new Node(3));

    EXPECT_EQ(nodeList.size(), 1);
    EXPECT_EQ(nodeList.front().children.size(), 2);
    EXPECT_EQ(nodeList.front().children.front()->value, 2);
}

class ThrowingObject {
public:
    static int constructor_call_count;
    static int copy_constructor_call_count;
    static int destructor_call_count;
    static bool should_throw;

    int value;

    explicit ThrowingObject(int v) : value(v) {
        if (should_throw && value == 42) {
            throw std::runtime_error("Constructor exception");
        }
        ++constructor_call_count;
    }

    ThrowingObject(const ThrowingObject& other) : value(other.value) {
        if (should_throw && value == 42) {
            throw std::runtime_error("Copy constructor exception");
        }
        ++copy_constructor_call_count;
    }

    ~ThrowingObject() {
        ++destructor_call_count;
    }
};

int ThrowingObject::constructor_call_count = 0;
int ThrowingObject::copy_constructor_call_count = 0;
int ThrowingObject::destructor_call_count = 0;
bool ThrowingObject::should_throw = false;

TEST_F(LinkedListTest, ExceptionSafety) {
    // Test strong exception safety during insertion
    LinkedList<ThrowingObject> throwingList;
    throwingList.push_back(ThrowingObject(1));
    throwingList.push_back(ThrowingObject(2));

    ThrowingObject::should_throw = true;

    // Test exception during push_back
    EXPECT_THROW({
        throwingList.push_back(ThrowingObject(42));
    }, std::runtime_error);

    // Verify list remains unchanged
    EXPECT_EQ(throwingList.size(), 2);
    EXPECT_EQ(throwingList.front().value, 1);
    EXPECT_EQ(throwingList.back().value, 2);

    // Reset counters for the next test
    ThrowingObject::constructor_call_count = 0;
    ThrowingObject::copy_constructor_call_count = 0;
    ThrowingObject::destructor_call_count = 0;

    // Test exception during copy construction
    ThrowingObject::should_throw = false;
    {
        LinkedList<ThrowingObject> normalList;
        normalList.push_back(ThrowingObject(1));
        normalList.push_back(ThrowingObject(42)); // This will throw when copied
        ThrowingObject::should_throw = true;

        EXPECT_THROW({
            LinkedList<ThrowingObject> copyList(normalList);
        }, std::runtime_error);
    }

    // Ensure all constructed objects were properly destroyed
    EXPECT_EQ(ThrowingObject::constructor_call_count + ThrowingObject::copy_constructor_call_count,
              ThrowingObject::destructor_call_count);
}

class PerformanceTests : public testing::Test {
protected:
    static constexpr const char *GREEN = "\033[32m";
    static constexpr const char *RED = "\033[31m";
    static constexpr const char *RESET = "\033[0m";

    template<typename Operation>
    static double measure_time(Operation op) {
        const auto start = std::chrono::high_resolution_clock::now();
        op();
        const auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    static void print_comparison(const std::string &operation, double ll_time, double std_time) {
        const double ratio = ll_time / std_time;
        const char *color = (ratio <= 1.0) ? GREEN : RED;
        std::cout << color << operation << ":\n"
                << "  LinkedList: " << ll_time << "ms\n"
                << "  std::list: " << std_time << "ms\n"
                << "  Ratio: " << ratio << "x" << RESET << "\n\n";
    }
};

TEST_F(PerformanceTests, OperationsComparison) {
    constexpr int size = 100000;

    std::vector<std::pair<std::string, std::function<void()> > > operations = {
        {
            "Push Back", [] {
                LinkedList<int> ll;
                std::list<int> stdl;

                auto llTime = measure_time([&]() {
                    for (int i = 0; i < size; ++i) ll.push_back(i);
                });

                auto stdTime = measure_time([&]() {
                    for (int i = 0; i < size; ++i) stdl.push_back(i);
                });

                print_comparison("Push Back", llTime, stdTime);
            }
        },
        {
            "Push Front", [] {
                LinkedList<int> ll;
                std::list<int> stdl;

                auto llTime = measure_time([&]() {
                    for (int i = 0; i < size; ++i) ll.push_front(i);
                });

                auto stdTime = measure_time([&]() {
                    for (int i = 0; i < size; ++i) stdl.push_front(i);
                });

                print_comparison("Push Front", llTime, stdTime);
            }
        },
        {
            "Insert Middle", [] {
                LinkedList<int> ll;
                std::list<int> stdl;
                for (int i = 0; i < size; ++i) {
                    ll.push_back(i);
                    stdl.push_back(i);
                }

                auto llTime = measure_time([&]() {
                    auto it = ll.begin();
                    std::advance(it, size / 2);
                    for (int i = 0; i < 1000; ++i) {
                        it = ll.insert(it, i);
                    }
                });

                auto stdTime = measure_time([&]() {
                    auto it = stdl.begin();
                    std::advance(it, size / 2);
                    for (int i = 0; i < 1000; ++i) {
                        it = stdl.insert(it, i);
                    }
                });

                print_comparison("Insert Middle", llTime, stdTime);
            }
        },
        {
            "Iteration", [] {
                LinkedList<int> ll;
                std::list<int> stdl;
                for (int i = 0; i < size; ++i) {
                    ll.push_back(i);
                    stdl.push_back(i);
                }

                auto llTime = measure_time([&]() {
                    volatile int sum = 0;
                    for (auto i: ll) sum += i;
                });

                auto stdTime = measure_time([&]() {
                    volatile int sum = 0;
                    for (const auto i: stdl) sum += i;
                });

                print_comparison("Iteration", llTime, stdTime);
            }
        }
    };

    std::cout << "\nPerformance Comparison (size = " << size << "):\n";
    std::cout << "----------------------------------------\n";

    for (const auto &op: operations | std::views::values) {
        op();
    }
}

TEST_F(PerformanceTests, DifferentSizes) {
    std::vector<int> sizes = {1000, 10000, 100000, 1000000};

    std::cout << "\nSize Scaling Comparison:\n";
    std::cout << "----------------------------------------\n";

    for (int size: sizes) {
        std::cout << "\nTesting size: " << size << "\n";

        LinkedList<int> ll;
        std::list<int> stdl;

        auto ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.push_back(i);
        });

        auto std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.push_back(i);
        });

        print_comparison("Push Back", ll_time, std_time);
    }
}

TEST_F(PerformanceTests, DifferentTypes) {
    constexpr int size = 100000;

    std::cout << "\nType Comparison (size = " << size << "):\n";
    std::cout << "----------------------------------------\n";

    // Test with int
    {
        LinkedList<int> ll;
        std::list<int> stdl;

        auto ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.push_back(i);
        });

        auto std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.push_back(i);
        });

        print_comparison("Int Type (push_back)", ll_time, std_time);

        ll.clear();
        stdl.clear();

        ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.emplace_back(i);
        });

        std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.emplace_back(i);
        });

        print_comparison("Int Type (emplace_back)", ll_time, std_time);
    }

    // Test with string
    {
        LinkedList<std::string> ll;
        std::list<std::string> stdl;

        auto ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.push_back("test" + std::to_string(i));
        });

        auto std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.push_back("test" + std::to_string(i));
        });

        print_comparison("String Type (push_back)", ll_time, std_time);

        ll.clear();
        stdl.clear();

        ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.emplace_back("test" + std::to_string(i));
        });

        std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.emplace_back("test" + std::to_string(i));
        });

        print_comparison("String Type (emplace_back)", ll_time, std_time);
    }

    // Test with a complex object
    {
        LinkedList<TestObject> ll;
        std::list<TestObject> stdl;

        auto ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.push_back(TestObject(i, "test"));
        });

        auto std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.push_back(TestObject(i, "test"));
        });

        print_comparison("Complex Object Type (push_back)", ll_time, std_time);

        ll.clear();
        stdl.clear();

        ll_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) ll.emplace_back(i, "test");
        });

        std_time = measure_time([&]() {
            for (int i = 0; i < size; ++i) stdl.emplace_back(i, "test");
        });

        print_comparison("Complex Object Type (emplace_back)", ll_time, std_time);
    }
}

TEST_F(PerformanceTests, EmplaceOperations) {
    constexpr int size = 100000;

    std::cout << "\nEmplace Operations Comparison (size = " << size << "):\n";
    std::cout << "----------------------------------------\n";

    LinkedList<TestObject> ll;
    std::list<TestObject> stdl;

    // Test emplace_back
    auto ll_time = measure_time([&]() {
        for (int i = 0; i < size; ++i) ll.emplace_back(i, "test");
    });

    auto std_time = measure_time([&]() {
        for (int i = 0; i < size; ++i) stdl.emplace_back(i, "test");
    });

    print_comparison("emplace_back", ll_time, std_time);

    ll.clear();
    stdl.clear();

    // Test emplace_front
    ll_time = measure_time([&]() {
        for (int i = 0; i < size; ++i) ll.emplace_front(i, "test");
    });

    std_time = measure_time([&]() {
        for (int i = 0; i < size; ++i) stdl.emplace_front(i, "test");
    });

    print_comparison("emplace_front", ll_time, std_time);

    ll.clear();
    stdl.clear();

    // Test emplace in the middle
    for (int i = 0; i < size; ++i) {
        ll.emplace_back(i, "test");
        stdl.emplace_back(i, "test");
    }

    ll_time = measure_time([&]() {
        auto it = ll.begin();
        std::advance(it, size / 2);
        for (int i = 0; i < 1000; ++i) {
            it = ll.emplace(it, i, "test");
        }
    });

    std_time = measure_time([&]() {
        auto it = stdl.begin();
        std::advance(it, size / 2);
        for (int i = 0; i < 1000; ++i) {
            it = stdl.emplace(it, i, "test");
        }
    });

    print_comparison("emplace middle", ll_time, std_time);
}
