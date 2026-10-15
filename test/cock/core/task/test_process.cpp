#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::task {

using core::hal::ArchProcessContext;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::Process;

namespace {

constexpr uint32_t FIRST_THREAD_ID = 7;
constexpr uint32_t SECOND_THREAD_ID = 8;
constexpr size_t NO_THREADS = 0;
constexpr size_t TWO_THREADS = 2;
constexpr uintptr_t REGION_START = 0x40000000;
constexpr size_t REGION_PAGES = 2;
constexpr size_t WRONG_REGION_PAGES = 3;
constexpr uint32_t KEYBOARD_DATA_PORT = 0x60;
constexpr uint32_t KEYBOARD_NEXT_PORT = 0x61;
constexpr uint32_t PIC_MASTER_PORT = 0x20;
constexpr uint32_t OVERFLOWING_FIRST_PORT = 0xFFFFFFF0;
constexpr uint32_t OVERFLOWING_COUNT = 0x20;
constexpr uint32_t SINGLE_PORT = 1;
constexpr uint32_t EMPTY_RANGE = 0;
constexpr uint32_t BITS_PER_BYTE = 8;

bool portGranted(const ArchProcessContext &context, uint32_t port) {
	return (context.iopm[port / BITS_PER_BYTE] &
			(1u << (port % BITS_PER_BYTE))) == 0;
}

void checkLifecycle() {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	EXPECT_FALSE(space.isNull());
	if (space.isNull()) return;

	Process *process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	EXPECT_FALSE(process->isKernel());
	EXPECT_EQ(NO_THREADS, process->getThreadCount());

	process->attachThread(FIRST_THREAD_ID);
	process->attachThread(SECOND_THREAD_ID);
	EXPECT_EQ(FIRST_THREAD_ID, process->getId());
	EXPECT_EQ(TWO_THREADS, process->getThreadCount());
	EXPECT_FALSE(process->detachThread());
	EXPECT_TRUE(process->detachThread());

	VirtAddr region(REGION_START);
	process->addMemoryRegion(region, REGION_PAGES);
	EXPECT_FALSE(process->takeMemoryRegion(region, WRONG_REGION_PAGES));
	EXPECT_TRUE(process->takeMemoryRegion(region, REGION_PAGES));
	EXPECT_FALSE(process->takeMemoryRegion(region, REGION_PAGES));

	delete process;
}

void checkPortGrants() {
	static ArchProcessContext context;
	core::hal::init_process_context(context);
	uint32_t initial_version = context.version;

	EXPECT_EQ(static_cast<uint32_t>(0), context.iopm_bytes);
	EXPECT_FALSE(portGranted(context, KEYBOARD_DATA_PORT));

	EXPECT_TRUE(core::hal::grant_hardware_resource(context, KEYBOARD_DATA_PORT,
												   SINGLE_PORT));
	EXPECT_TRUE(context.version != initial_version);
	EXPECT_EQ(KEYBOARD_DATA_PORT / BITS_PER_BYTE + 1, context.iopm_bytes);
	EXPECT_TRUE(portGranted(context, KEYBOARD_DATA_PORT));
	EXPECT_FALSE(portGranted(context, KEYBOARD_NEXT_PORT));

	EXPECT_FALSE(
		core::hal::grant_hardware_resource(context, PIC_MASTER_PORT, SINGLE_PORT));
	EXPECT_FALSE(core::hal::grant_hardware_resource(
		context, OVERFLOWING_FIRST_PORT, OVERFLOWING_COUNT));
	EXPECT_FALSE(core::hal::grant_hardware_resource(context, KEYBOARD_DATA_PORT,
													EMPTY_RANGE));
}

} // namespace

void test_process() {
	TestRunner::begin_suite("Process and hardware permissions");
	checkLifecycle();
	checkPortGrants();
	TestRunner::end_suite();
}

} // namespace cock::test::task
