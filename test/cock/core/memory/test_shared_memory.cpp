#include <cock/core/memory/address.hpp>
#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::memory {

using core::memory::NO_SHARED_OBJECT;
using core::memory::PhysAddr;
using core::memory::SharedMemory;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::Process;

namespace {

constexpr uint32_t CREATOR_PID = 101;
constexpr uint32_t READER_PID = 102;
constexpr uint32_t THIRD_PID = 103;
constexpr uint32_t STRANGER_PID = 104;
constexpr size_t OBJECT_PAGES = 3;
constexpr size_t NO_PAGES = 0;
constexpr size_t SINGLE_PAGE = 1;
constexpr size_t NO_OBJECTS = 0;
constexpr uint32_t UNKNOWN_OBJECT = SharedMemory::MAX_OBJECTS + 1;
constexpr size_t PAGES_LEFT_IN_QUOTA = 2;

Process *makeProcess(uint32_t pid) {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	if (space.isNull()) return nullptr;

	Process *process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	process->attachThread(pid);
	return process;
}

void checkRights() {
	Process *creator = makeProcess(CREATOR_PID);
	EXPECT_NOT_NULL(creator);
	if (creator == nullptr) return;

	size_t baseline = SharedMemory::objectCount();
	uint32_t object = SharedMemory::create(*creator, OBJECT_PAGES);
	EXPECT_NEQ(NO_SHARED_OBJECT, object);
	EXPECT_EQ(baseline + 1, SharedMemory::objectCount());
	EXPECT_EQ(OBJECT_PAGES, SharedMemory::pageCount(object));
	EXPECT_EQ(OBJECT_PAGES, creator->getOwnedPages());

	for (size_t idx = 0; idx < OBJECT_PAGES; idx++) {
		EXPECT_FALSE(SharedMemory::frame(object, idx).isNull());
		for (size_t other = 0; other < idx; other++)
			EXPECT_NEQ(SharedMemory::frame(object, other).raw(),
					   SharedMemory::frame(object, idx).raw());
	}
	EXPECT_TRUE(SharedMemory::frame(object, OBJECT_PAGES).isNull());

	EXPECT_TRUE(SharedMemory::mayMap(object, CREATOR_PID, true));
	EXPECT_FALSE(SharedMemory::mayMap(object, READER_PID, false));
	EXPECT_FALSE(SharedMemory::share(object, STRANGER_PID, READER_PID, false));

	EXPECT_TRUE(SharedMemory::share(object, CREATOR_PID, READER_PID, false));
	EXPECT_TRUE(SharedMemory::mayMap(object, READER_PID, false));
	EXPECT_FALSE(SharedMemory::mayMap(object, READER_PID, true));
	EXPECT_FALSE(SharedMemory::share(object, READER_PID, THIRD_PID, true));
	EXPECT_TRUE(SharedMemory::share(object, READER_PID, THIRD_PID, false));
	EXPECT_FALSE(SharedMemory::mayMap(object, THIRD_PID, true));

	EXPECT_TRUE(SharedMemory::share(object, CREATOR_PID, READER_PID, true));
	EXPECT_TRUE(SharedMemory::mayMap(object, READER_PID, true));

	EXPECT_FALSE(SharedMemory::release(object, STRANGER_PID));
	EXPECT_TRUE(SharedMemory::release(object, READER_PID));
	EXPECT_FALSE(SharedMemory::mayMap(object, READER_PID, false));
	EXPECT_TRUE(SharedMemory::release(object, THIRD_PID));
	EXPECT_EQ(baseline + 1, SharedMemory::objectCount());

	EXPECT_TRUE(SharedMemory::release(object, CREATOR_PID));
	EXPECT_EQ(baseline, SharedMemory::objectCount());
	EXPECT_EQ(NO_PAGES, creator->getOwnedPages());
	EXPECT_EQ(NO_PAGES, SharedMemory::pageCount(object));
	EXPECT_FALSE(SharedMemory::release(object, CREATOR_PID));

	delete creator;
}

void checkMappingsKeepTheObject() {
	Process *creator = makeProcess(CREATOR_PID);
	EXPECT_NOT_NULL(creator);
	if (creator == nullptr) return;

	size_t baseline = SharedMemory::objectCount();
	uint32_t object = SharedMemory::create(*creator, SINGLE_PAGE);
	EXPECT_NEQ(NO_SHARED_OBJECT, object);

	SharedMemory::mapped(object);
	EXPECT_TRUE(SharedMemory::release(object, CREATOR_PID));
	EXPECT_EQ(baseline + 1, SharedMemory::objectCount());
	EXPECT_FALSE(SharedMemory::frame(object, 0).isNull());
	EXPECT_FALSE(SharedMemory::mayMap(object, CREATOR_PID, false));

	SharedMemory::unmapped(object);
	EXPECT_EQ(baseline, SharedMemory::objectCount());
	EXPECT_EQ(NO_PAGES, creator->getOwnedPages());

	delete creator;
}

void checkProcessDeath() {
	Process *creator = makeProcess(CREATOR_PID);
	Process *reader = makeProcess(READER_PID);
	EXPECT_NOT_NULL(creator);
	EXPECT_NOT_NULL(reader);
	if (creator == nullptr || reader == nullptr) return;

	size_t baseline = SharedMemory::objectCount();
	uint32_t object = SharedMemory::create(*creator, SINGLE_PAGE);
	EXPECT_NEQ(NO_SHARED_OBJECT, object);
	EXPECT_TRUE(SharedMemory::share(object, CREATOR_PID, READER_PID, false));

	delete creator;
	EXPECT_EQ(baseline + 1, SharedMemory::objectCount());
	EXPECT_FALSE(SharedMemory::mayMap(object, CREATOR_PID, false));
	EXPECT_TRUE(SharedMemory::mayMap(object, READER_PID, false));

	delete reader;
	EXPECT_EQ(baseline, SharedMemory::objectCount());
}

void checkLimits() {
	Process *creator = makeProcess(CREATOR_PID);
	EXPECT_NOT_NULL(creator);
	if (creator == nullptr) return;

	EXPECT_EQ(NO_SHARED_OBJECT, SharedMemory::create(*creator, NO_PAGES));
	EXPECT_EQ(NO_SHARED_OBJECT,
			  SharedMemory::create(*creator, SharedMemory::MAX_OBJECT_PAGES + 1));
	EXPECT_EQ(NO_PAGES, creator->getOwnedPages());
	EXPECT_FALSE(SharedMemory::mayMap(NO_SHARED_OBJECT, CREATOR_PID, false));
	EXPECT_FALSE(SharedMemory::mayMap(UNKNOWN_OBJECT, CREATOR_PID, false));

	size_t baseline = SharedMemory::objectCount();
	uint32_t objects[SharedMemory::MAX_OBJECTS];
	size_t created = 0;
	while (created < SharedMemory::MAX_OBJECTS) {
		uint32_t object = SharedMemory::create(*creator, SINGLE_PAGE);
		if (object == NO_SHARED_OBJECT) break;
		objects[created++] = object;
	}
	EXPECT_EQ(SharedMemory::MAX_OBJECTS - baseline, created);
	EXPECT_EQ(NO_SHARED_OBJECT, SharedMemory::create(*creator, SINGLE_PAGE));
	EXPECT_EQ(created, creator->getOwnedPages());

	for (size_t idx = 0; idx < created; idx++)
		EXPECT_TRUE(SharedMemory::release(objects[idx], CREATOR_PID));
	EXPECT_EQ(baseline, SharedMemory::objectCount());
	EXPECT_EQ(NO_PAGES, creator->getOwnedPages());

	EXPECT_TRUE(creator->reservePages(Process::MAX_OWNED_PAGES -
									  PAGES_LEFT_IN_QUOTA));
	EXPECT_EQ(NO_SHARED_OBJECT, SharedMemory::create(*creator, OBJECT_PAGES));
	EXPECT_EQ(baseline, SharedMemory::objectCount());
	creator->returnPages(Process::MAX_OWNED_PAGES - PAGES_LEFT_IN_QUOTA);
	EXPECT_EQ(NO_PAGES, creator->getOwnedPages());

	delete creator;
}

} // namespace

void test_shared_memory() {
	TestRunner::begin_suite("Shared memory");
	checkRights();
	checkMappingsKeepTheObject();
	checkProcessDeath();
	checkLimits();
	TestRunner::end_suite();
}

} // namespace cock::test::memory
