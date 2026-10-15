#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::arch {

using cock::arch::x86::PAGE_SIZE;
using cock::arch::x86::PhysicalMemoryManager;
using core::memory::PhysAddr;

namespace {

constexpr size_t BATCH_FRAMES = 64;
constexpr uint32_t LOW_MEMORY_END = 0x100000;
constexpr uint32_t VGA_TEXT_BUFFER = 0xB8000;
constexpr uint32_t LOCAL_APIC_BASE = 0xFEE00000;

void checkAllocateAndFree() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	EXPECT_TRUE(before > 0);

	PhysAddr frame = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(frame.isNull());
	EXPECT_EQ(static_cast<size_t>(0), frame.pageOffset());
	EXPECT_EQ(before - 1, PhysicalMemoryManager::freeFrameCount());

	PhysicalMemoryManager::freeFrame(frame);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkFrameIsReused() {
	PhysAddr first = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(first.isNull());
	PhysicalMemoryManager::freeFrame(first);

	PhysAddr second = PhysicalMemoryManager::allocFrame();
	EXPECT_TRUE(first == second);
	PhysicalMemoryManager::freeFrame(second);
}

void checkReservedRanges() {
	PhysAddr batch[BATCH_FRAMES];
	for (size_t idx = 0; idx < BATCH_FRAMES; idx++) {
		batch[idx] = PhysicalMemoryManager::allocFrame();
		EXPECT_FALSE(batch[idx].isNull());
		EXPECT_TRUE(batch[idx].raw() >= LOW_MEMORY_END);
	}
	for (size_t idx = 0; idx < BATCH_FRAMES; idx++)
		PhysicalMemoryManager::freeFrame(batch[idx]);
}

void checkFramesAreDistinct() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr batch[BATCH_FRAMES];
	for (size_t idx = 0; idx < BATCH_FRAMES; idx++)
		batch[idx] = PhysicalMemoryManager::allocFrame();

	bool distinct = true;
	for (size_t outer = 0; outer < BATCH_FRAMES; outer++)
		for (size_t inner = outer + 1; inner < BATCH_FRAMES; inner++)
			if (batch[outer] == batch[inner]) distinct = false;
	EXPECT_TRUE(distinct);
	EXPECT_EQ(before - BATCH_FRAMES, PhysicalMemoryManager::freeFrameCount());

	for (size_t idx = 0; idx < BATCH_FRAMES; idx++)
		PhysicalMemoryManager::freeFrame(batch[idx]);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkDoubleFree() {
	PhysAddr frame = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(frame.isNull());

	PhysicalMemoryManager::freeFrame(frame);
	size_t afterFirst = PhysicalMemoryManager::freeFrameCount();

	PhysicalMemoryManager::freeFrame(frame);
	EXPECT_EQ(afterFirst, PhysicalMemoryManager::freeFrameCount());
}

void checkFreeOutOfRange() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysicalMemoryManager::freeFrame(PhysAddr(LOCAL_APIC_BASE));
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkIsRam() {
	PhysAddr frame = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(frame.isNull());
	EXPECT_TRUE(PhysicalMemoryManager::isRam(frame));
	EXPECT_TRUE(PhysicalMemoryManager::isRam(frame + (PAGE_SIZE - 1)));
	PhysicalMemoryManager::freeFrame(frame);
	EXPECT_TRUE(PhysicalMemoryManager::isRam(frame));

	EXPECT_FALSE(PhysicalMemoryManager::isRam(PhysAddr(VGA_TEXT_BUFFER)));
	EXPECT_FALSE(PhysicalMemoryManager::isRam(PhysAddr(LOCAL_APIC_BASE)));
}

} // namespace

void test_pmm() {
	TestRunner::begin_suite("Physical Memory Manager");
	checkAllocateAndFree();
	checkFrameIsReused();
	checkReservedRanges();
	checkFramesAreDistinct();
	checkDoubleFree();
	checkFreeOutOfRange();
	checkIsRam();
	TestRunner::end_suite();
}

} // namespace cock::test::arch
