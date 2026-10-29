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
constexpr uint32_t RESERVED_LOW_FRAME = 0x9F000;
constexpr uint32_t KERNEL_IMAGE_FRAME = 0x100000;
constexpr uint32_t NO_REFERENCES = 0;
constexpr uint32_t ONE_REFERENCE = 1;
constexpr uint32_t TWO_REFERENCES = 2;

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

void checkFreeReservedFrame() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysicalMemoryManager::freeFrame(PhysAddr(RESERVED_LOW_FRAME));
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
	PhysicalMemoryManager::freeFrame(PhysAddr(KERNEL_IMAGE_FRAME));
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

void checkSharedFrame() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr frame = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(frame.isNull());
	EXPECT_EQ(ONE_REFERENCE, PhysicalMemoryManager::frameReferences(frame));

	EXPECT_TRUE(PhysicalMemoryManager::shareFrame(frame));
	EXPECT_EQ(TWO_REFERENCES, PhysicalMemoryManager::frameReferences(frame));

	PhysicalMemoryManager::freeFrame(frame);
	EXPECT_EQ(ONE_REFERENCE, PhysicalMemoryManager::frameReferences(frame));
	EXPECT_EQ(before - 1, PhysicalMemoryManager::freeFrameCount());

	PhysicalMemoryManager::freeFrame(frame);
	EXPECT_EQ(NO_REFERENCES, PhysicalMemoryManager::frameReferences(frame));
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
	EXPECT_FALSE(PhysicalMemoryManager::shareFrame(frame));
}

void checkReferenceLimit() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr frame = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(frame.isNull());

	for (uint32_t count = ONE_REFERENCE; count < cock::arch::x86::MAX_FRAME_REFERENCES; count++)
		EXPECT_TRUE(PhysicalMemoryManager::shareFrame(frame));
	EXPECT_EQ(cock::arch::x86::MAX_FRAME_REFERENCES, PhysicalMemoryManager::frameReferences(frame));
	EXPECT_FALSE(PhysicalMemoryManager::shareFrame(frame));
	EXPECT_EQ(cock::arch::x86::MAX_FRAME_REFERENCES, PhysicalMemoryManager::frameReferences(frame));

	for (uint32_t count = NO_REFERENCES; count < cock::arch::x86::MAX_FRAME_REFERENCES; count++)
		PhysicalMemoryManager::freeFrame(frame);
	EXPECT_EQ(NO_REFERENCES, PhysicalMemoryManager::frameReferences(frame));
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
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
	checkFreeReservedFrame();
	checkIsRam();
	checkSharedFrame();
	checkReferenceLimit();
	TestRunner::end_suite();
}

} // namespace cock::test::arch
