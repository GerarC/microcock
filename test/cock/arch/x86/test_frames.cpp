#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/vmm.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::arch {

using cock::arch::x86::PhysicalMemoryManager;
using core::memory::PhysAddr;
using core::memory::VirtualMemoryManager;

namespace {

constexpr size_t BATCH_FRAMES = 8;
constexpr size_t NO_FRAMES = 0;
constexpr size_t SINGLE_FRAME = 1;
constexpr size_t FRAMES_BEYOND_MEMORY = 0x100000;

void checkBatchBalance() {
	PhysAddr frames[BATCH_FRAMES];
	size_t before = PhysicalMemoryManager::freeFrameCount();

	EXPECT_TRUE(VirtualMemoryManager::allocFrames(frames, BATCH_FRAMES));
	EXPECT_EQ(before - BATCH_FRAMES, PhysicalMemoryManager::freeFrameCount());

	for (size_t idx = 0; idx < BATCH_FRAMES; idx++) {
		EXPECT_FALSE(frames[idx].isNull());
		EXPECT_EQ(static_cast<size_t>(0), frames[idx].pageOffset());
		EXPECT_TRUE(PhysicalMemoryManager::isRam(frames[idx]));
		for (size_t other = 0; other < idx; other++)
			EXPECT_NEQ(frames[other].raw(), frames[idx].raw());
	}

	VirtualMemoryManager::freeFrames(frames, BATCH_FRAMES);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkRefusals() {
	PhysAddr frames[SINGLE_FRAME];
	size_t before = PhysicalMemoryManager::freeFrameCount();

	EXPECT_FALSE(VirtualMemoryManager::allocFrames(frames, NO_FRAMES));
	EXPECT_FALSE(VirtualMemoryManager::allocFrames(nullptr, SINGLE_FRAME));
	EXPECT_FALSE(VirtualMemoryManager::allocFrames(frames, FRAMES_BEYOND_MEMORY));
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

} // namespace

void test_frames() {
	TestRunner::begin_suite("Frames without a mapping");
	checkBatchBalance();
	checkRefusals();
	TestRunner::end_suite();
}

} // namespace cock::test::arch
