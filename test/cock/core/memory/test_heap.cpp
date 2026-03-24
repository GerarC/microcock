#include <test/test_framework.hpp>
#include <cock/core/memory/heap.hpp>
#include <string.h>
#include <stdint.h>

namespace cock::test::memory {

using core::memory::kmalloc;
using core::memory::kfree;

void test_heap() {
    TestRunner::begin_suite("Heap Allocator (kmalloc/kfree)");

    void* ptr1 = kmalloc(32);
    EXPECT_NOT_NULL(ptr1);

    if (ptr1 != nullptr) {
        memset(ptr1, 0xAA, 32);
        uint8_t* bytes = reinterpret_cast<uint8_t*>(ptr1);
        EXPECT_EQ(0xAA, bytes[0]);
        EXPECT_EQ(0xAA, bytes[31]);
    }

    void* ptr2 = kmalloc(4096);
    EXPECT_NOT_NULL(ptr2);
    EXPECT_NEQ(ptr1, ptr2); 

    kfree(ptr1);
    kfree(ptr2);
    EXPECT_TRUE(true); 

    TestRunner::end_suite();
}

} // namespace cock::test::memory
