#include <test/test_framework.hpp>

static int constructor_calls = 0;
static int destructor_calls = 0;

namespace cock::test::memory {

class DummyObject {
public:
    int value;

    DummyObject() {
        value = 42; 
        constructor_calls++;
    }

    ~DummyObject() {
        destructor_calls++;
    }
};

void test_new_operator() {
    TestRunner::begin_suite("C++ new/delete Operators");

    constructor_calls = 0;
    destructor_calls = 0;

    DummyObject* obj = new DummyObject();
    
    EXPECT_NOT_NULL(obj);
    EXPECT_EQ(42, obj->value);
    EXPECT_EQ(1, constructor_calls);

    delete obj;
    EXPECT_EQ(1, destructor_calls);

    constructor_calls = 0;
    DummyObject* arr = new DummyObject[3];

    EXPECT_NOT_NULL(arr);
    EXPECT_EQ(3, constructor_calls);
    EXPECT_EQ(42, arr[0].value);
    EXPECT_EQ(42, arr[1].value);
    EXPECT_EQ(42, arr[2].value);

    destructor_calls = 0;
    delete[] arr;

    EXPECT_EQ(3, destructor_calls);

    TestRunner::end_suite();
}

} // namespace cock::test::memory
