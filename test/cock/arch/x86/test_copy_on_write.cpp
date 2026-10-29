#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <test/test_framework.hpp>

namespace cock::test::arch {

using cock::arch::x86::PAGE_SIZE;
using cock::arch::x86::PageFlag;
using cock::arch::x86::Paging;
using cock::arch::x86::PhysicalMemoryManager;
using core::memory::PhysAddr;
using core::memory::USER_SPACE_END;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::memory::VMMPermission;
using core::task::Process;

namespace {

constexpr uintptr_t PRIVATE_PAGE = 0x40000000;
constexpr uintptr_t SHARED_PAGE = PRIVATE_PAGE + PAGE_SIZE;
constexpr uint32_t PARENT_VALUE = 0x1111AAAA;
constexpr uint32_t CHILD_VALUE = 0x2222BBBB;
constexpr uint32_t SHARED_VALUE = 0x3333CCCC;
constexpr uint32_t SHARED_WRITTEN = 0x4444DDDD;
constexpr uint32_t ONE_REFERENCE = 1;
constexpr uint32_t TWO_REFERENCES = 2;
constexpr size_t ONE_PAGE = 1;
constexpr size_t TWO_PAGES = 2;
constexpr size_t FEW_BYTES = sizeof(uint32_t);
constexpr size_t SMALLEST_RESERVE = 1;
constexpr char SAMPLE_NAME[] = "sample";

class InSpace {
  public:
	explicit InSpace(PhysAddr space) : previous(core::hal::get_current_address_space()) {
		core::hal::switch_address_space(space);
	}
	~InSpace() { core::hal::switch_address_space(previous); }
	InSpace(const InSpace &) = delete;
	InSpace &operator=(const InSpace &) = delete;

  private:
	core::hal::InterruptGuard guard;
	PhysAddr previous;
};

uint32_t &wordAt(uintptr_t address) { return *reinterpret_cast<uint32_t *>(address); }

uint32_t flagsAt(uintptr_t address) { return Paging::getPageFlags(VirtAddr(address)); }

PhysAddr frameAt(uintptr_t address) { return Paging::translate(VirtAddr(address)); }

bool writable(uint32_t flags) { return (flags & PageFlag::WRITEABLE) != 0; }

bool copyOnWrite(uint32_t flags) { return (flags & PageFlag::COPY_ON_WRITE) != 0; }

PhysAddr makeParent(PhysAddr &frame) {
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	if (space.isNull()) return space;

	InSpace scope(space);
	VirtAddr page = VirtualMemoryManager::mapPagesAt(VirtAddr(PRIVATE_PAGE), ONE_PAGE,
													 VMMPermission::USER_DATA, USER_SPACE_END);
	EXPECT_FALSE(page.isNull());
	wordAt(PRIVATE_PAGE) = PARENT_VALUE;
	frame = frameAt(PRIVATE_PAGE);
	return space;
}

void expectSharedCopyOnWrite(PhysAddr space, PhysAddr frame) {
	InSpace scope(space);
	uint32_t flags = flagsAt(PRIVATE_PAGE);
	EXPECT_FALSE(writable(flags));
	EXPECT_TRUE(copyOnWrite(flags));
	EXPECT_TRUE(frameAt(PRIVATE_PAGE) == frame);
	EXPECT_EQ(PARENT_VALUE, wordAt(PRIVATE_PAGE));
}

void checkCloneSharesTheFrame() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr frame;
	PhysAddr parent = makeParent(frame);
	EXPECT_FALSE(parent.isNull());
	if (parent.isNull()) return;

	PhysAddr child = VirtualMemoryManager::cloneAddressSpace(parent);
	EXPECT_FALSE(child.isNull());
	if (!child.isNull()) {
		EXPECT_EQ(TWO_REFERENCES, PhysicalMemoryManager::frameReferences(frame));
		expectSharedCopyOnWrite(parent, frame);
		expectSharedCopyOnWrite(child, frame);
		VirtualMemoryManager::destroyAddressSpace(child);
		EXPECT_EQ(ONE_REFERENCE, PhysicalMemoryManager::frameReferences(frame));
	}

	VirtualMemoryManager::destroyAddressSpace(parent);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkWriteGivesAPrivateCopy() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr frame;
	PhysAddr parent = makeParent(frame);
	EXPECT_FALSE(parent.isNull());
	if (parent.isNull()) return;
	PhysAddr child = VirtualMemoryManager::cloneAddressSpace(parent);
	EXPECT_FALSE(child.isNull());
	if (child.isNull()) {
		VirtualMemoryManager::destroyAddressSpace(parent);
		return;
	}

	{
		InSpace scope(child);
		EXPECT_TRUE(VirtualMemoryManager::resolveCopyOnWrite(VirtAddr(PRIVATE_PAGE)));
		uint32_t flags = flagsAt(PRIVATE_PAGE);
		EXPECT_TRUE(writable(flags));
		EXPECT_FALSE(copyOnWrite(flags));
		EXPECT_FALSE(frameAt(PRIVATE_PAGE) == frame);
		EXPECT_EQ(PARENT_VALUE, wordAt(PRIVATE_PAGE));
		wordAt(PRIVATE_PAGE) = CHILD_VALUE;
		EXPECT_FALSE(VirtualMemoryManager::resolveCopyOnWrite(VirtAddr(PRIVATE_PAGE)));
	}
	EXPECT_EQ(ONE_REFERENCE, PhysicalMemoryManager::frameReferences(frame));

	{
		InSpace scope(parent);
		EXPECT_EQ(PARENT_VALUE, wordAt(PRIVATE_PAGE));
		EXPECT_TRUE(VirtualMemoryManager::resolveCopyOnWrite(VirtAddr(PRIVATE_PAGE)));
		EXPECT_TRUE(frameAt(PRIVATE_PAGE) == frame);
		EXPECT_TRUE(writable(flagsAt(PRIVATE_PAGE)));
		EXPECT_FALSE(copyOnWrite(flagsAt(PRIVATE_PAGE)));
		EXPECT_EQ(PARENT_VALUE, wordAt(PRIVATE_PAGE));
	}

	VirtualMemoryManager::destroyAddressSpace(child);
	VirtualMemoryManager::destroyAddressSpace(parent);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkAKernelWriteResolvesThePage() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr frame;
	PhysAddr parent = makeParent(frame);
	EXPECT_FALSE(parent.isNull());
	if (parent.isNull()) return;
	PhysAddr child = VirtualMemoryManager::cloneAddressSpace(parent);
	EXPECT_FALSE(child.isNull());
	if (child.isNull()) {
		VirtualMemoryManager::destroyAddressSpace(parent);
		return;
	}

	{
		InSpace scope(parent);
		EXPECT_TRUE(VirtualMemoryManager::isUserRangeValid(VirtAddr(PRIVATE_PAGE), FEW_BYTES, false,
														   USER_SPACE_END));
		EXPECT_TRUE(copyOnWrite(flagsAt(PRIVATE_PAGE)));
	}
	{
		InSpace scope(child);
		EXPECT_TRUE(VirtualMemoryManager::isUserRangeValid(VirtAddr(PRIVATE_PAGE), FEW_BYTES, true,
														   USER_SPACE_END));
		EXPECT_TRUE(writable(flagsAt(PRIVATE_PAGE)));
		EXPECT_FALSE(frameAt(PRIVATE_PAGE) == frame);
		wordAt(PRIVATE_PAGE) = CHILD_VALUE;
	}
	{
		InSpace scope(parent);
		EXPECT_EQ(PARENT_VALUE, wordAt(PRIVATE_PAGE));
	}

	VirtualMemoryManager::destroyAddressSpace(child);
	VirtualMemoryManager::destroyAddressSpace(parent);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkSharedMemoryStaysShared() {
	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr parent = VirtualMemoryManager::createAddressSpace();
	PhysAddr shared = PhysicalMemoryManager::allocFrame();
	EXPECT_FALSE(parent.isNull());
	EXPECT_FALSE(shared.isNull());
	if (parent.isNull() || shared.isNull()) return;

	{
		InSpace scope(parent);
		EXPECT_TRUE(VirtualMemoryManager::map(shared, VirtAddr(SHARED_PAGE),
											  VMMPermission::USER_DATA | VMMPermission::SHARED));
		wordAt(SHARED_PAGE) = SHARED_VALUE;
	}
	PhysAddr child = VirtualMemoryManager::cloneAddressSpace(parent);
	EXPECT_FALSE(child.isNull());
	if (!child.isNull()) {
		{
			InSpace scope(child);
			EXPECT_TRUE(writable(flagsAt(SHARED_PAGE)));
			EXPECT_FALSE(copyOnWrite(flagsAt(SHARED_PAGE)));
			EXPECT_TRUE(frameAt(SHARED_PAGE) == shared);
			EXPECT_EQ(SHARED_VALUE, wordAt(SHARED_PAGE));
			wordAt(SHARED_PAGE) = SHARED_WRITTEN;
		}
		{
			InSpace scope(parent);
			EXPECT_TRUE(writable(flagsAt(SHARED_PAGE)));
			EXPECT_EQ(SHARED_WRITTEN, wordAt(SHARED_PAGE));
		}
		EXPECT_EQ(ONE_REFERENCE, PhysicalMemoryManager::frameReferences(shared));
		VirtualMemoryManager::destroyAddressSpace(child);
	}

	VirtualMemoryManager::destroyAddressSpace(parent);
	PhysicalMemoryManager::freeFrame(shared);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkCloneRefusals() {
	EXPECT_TRUE(VirtualMemoryManager::cloneAddressSpace(PhysAddr()).isNull());
	EXPECT_TRUE(VirtualMemoryManager::cloneAddressSpace(VirtualMemoryManager::getKernelDirectory())
					.isNull());

	size_t before = PhysicalMemoryManager::freeFrameCount();
	PhysAddr parent = VirtualMemoryManager::createAddressSpace();
	EXPECT_FALSE(parent.isNull());
	if (parent.isNull()) return;
	{
		InSpace scope(parent);
		EXPECT_TRUE(VirtualMemoryManager::cloneAddressSpace(parent).isNull());
	}
	VirtualMemoryManager::destroyAddressSpace(parent);
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
}

void checkAFailedCloneLeavesNothingBehind() {
	PhysAddr frame;
	PhysAddr parent = makeParent(frame);
	EXPECT_FALSE(parent.isNull());
	if (parent.isNull()) return;

	size_t before = PhysicalMemoryManager::freeFrameCount();
	EXPECT_TRUE(Paging::cloneAddressSpace(parent, before).isNull());
	EXPECT_TRUE(Paging::cloneAddressSpace(parent, before - SMALLEST_RESERVE).isNull());
	EXPECT_EQ(before, PhysicalMemoryManager::freeFrameCount());
	EXPECT_EQ(ONE_REFERENCE, PhysicalMemoryManager::frameReferences(frame));
	{
		InSpace scope(parent);
		EXPECT_TRUE(writable(flagsAt(PRIVATE_PAGE)));
		EXPECT_FALSE(copyOnWrite(flagsAt(PRIVATE_PAGE)));
	}

	VirtualMemoryManager::destroyAddressSpace(parent);
}

void checkBookkeepingIsCopied() {
	Process *parent = Process::create(VirtualMemoryManager::createAddressSpace(),
									  VirtAddr(core::hal::USER_SPACE_LIMIT));
	Process *child = Process::create(VirtualMemoryManager::createAddressSpace(),
									 VirtAddr(core::hal::USER_SPACE_LIMIT));
	parent->setName(SAMPLE_NAME);
	EXPECT_TRUE(parent->addMemoryRegion(VirtAddr(PRIVATE_PAGE), TWO_PAGES, true));

	child->copyImageFrom(*parent);
	EXPECT_EQ(parent->getRegionCount(), child->getRegionCount());
	EXPECT_EQ(parent->getOwnedPages(), child->getOwnedPages());
	EXPECT_EQ(strcmp(SAMPLE_NAME, child->getName()), 0);
	EXPECT_EQ(parent->getRegion(0).virt_addr, child->getRegion(0).virt_addr);
	EXPECT_EQ(parent->getRegion(0).page_count, child->getRegion(0).page_count);

	delete child;
	delete parent;
}

} // namespace

void test_copy_on_write() {
	TestRunner::begin_suite("Copy on write");
	checkCloneSharesTheFrame();
	checkWriteGivesAPrivateCopy();
	checkAKernelWriteResolvesThePage();
	checkSharedMemoryStaysShared();
	checkCloneRefusals();
	checkAFailedCloneLeavesNothingBehind();
	checkBookkeepingIsCopied();
	TestRunner::end_suite();
}

} // namespace cock::test::arch
