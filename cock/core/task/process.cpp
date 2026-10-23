#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/slab_pool.hpp>
#include <cock/utils/panic.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <hal/address_space.hpp>
#include <hal/page.hpp>

namespace cock::core::task {

using memory::NO_SHARED_OBJECT;
using memory::PhysAddr;
using memory::SharedMemory;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using utils::panic;

namespace {

constexpr size_t PROCESS_POOL_CHUNK_PAGES = 4;

using ProcessPool = memory::SlabPool<Process, PROCESS_POOL_CHUNK_PAGES>;

} // namespace

Process::Process(PhysAddr address_space, VirtAddr user_space_limit,
				 bool is_kernel)
	: kernel(is_kernel), addressSpace(address_space),
	  userSpaceLimit(user_space_limit) {
	hal::init_process_context(context);
}

void *Process::operator new(size_t size) {
	if (size != sizeof(Process)) panic("process allocation of the wrong size");
	void *slot = ProcessPool::allocate();
	if (slot == nullptr) panic("process pool out of memory");
	return slot;
}

void Process::operator delete(void *pointer) { ProcessPool::release(pointer); }

Process::~Process() {
	hal::release_process_context(context);
	if (kernel) return;

	VirtualMemoryManager::destroyAddressSpace(addressSpace);
	for (size_t idx = 0; idx < regionCount; idx++) {
		if (regions[idx].shared_object != NO_SHARED_OBJECT)
			SharedMemory::unmapped(regions[idx].shared_object);
	}
	SharedMemory::dropProcess(*this);
}

Process *Process::createKernel() {
	return new Process(VirtualMemoryManager::getKernelDirectory(), VirtAddr(),
					   true);
}

Process *Process::create(PhysAddr address_space, VirtAddr user_space_limit) {
	return new Process(address_space, user_space_limit, false);
}

void Process::setName(const char *new_name) {
	size_t length = 0;
	while (new_name != nullptr && new_name[length] != '\0' &&
		   length < MAX_NAME_LENGTH - 1) {
		name[length] = new_name[length];
		length++;
	}
	name[length] = '\0';
}

void Process::attachThread(uint32_t thread_id) {
	if (threadCount == 0) id = thread_id;
	threadCount++;
	liveThreads++;
}

bool Process::threadExited() {
	if (liveThreads > 0) liveThreads--;
	return liveThreads == 0;
}

bool Process::detachThread() {
	if (threadCount > 0) threadCount--;
	return threadCount == 0;
}

bool Process::reserveExitNotice() {
	if (!canReserveExitNotice()) return false;
	notifiedChildren++;
	return true;
}

bool Process::recordExitNotice(uint32_t child, uint32_t status) {
	return exitNotices.push({child, status});
}

bool Process::takeExitNotice(ExitNotice &notice) {
	if (!exitNotices.pop(notice)) return false;
	if (notifiedChildren > 0) notifiedChildren--;
	return true;
}

bool Process::reservePages(size_t pages) {
	if (!canOwnPages(pages)) return false;
	ownedPages += pages;
	return true;
}

void Process::returnPages(size_t pages) {
	ownedPages -= pages < ownedPages ? pages : ownedPages;
}

bool Process::addMemoryRegion(VirtAddr addr, size_t pages, bool owned,
							  uint32_t shared_object) {
	if (!hasRegionSlot() || pages == 0) return false;
	if (owned && !canOwnPages(pages)) return false;

	regions[regionCount++] = {addr.raw(), pages, owned, shared_object};
	if (owned) ownedPages += pages;
	return true;
}

bool Process::takeMemoryRegion(VirtAddr addr, size_t pages,
							   uint32_t *shared_object) {
	for (size_t idx = 0; idx < regionCount; idx++) {
		if (regions[idx].virt_addr != addr.raw()) continue;
		if (regions[idx].page_count != pages) return false;

		if (shared_object != nullptr)
			*shared_object = regions[idx].shared_object;
		if (regions[idx].owned) ownedPages -= pages;
		regions[idx] = regions[--regionCount];
		return true;
	}
	return false;
}

VirtAddr Process::findFreeRange(size_t pages, VirtAddr from,
								VirtAddr limit) const {
	uint64_t length = static_cast<uint64_t>(pages) << hal::PAGE_SHIFT;
	uint64_t candidate = from.raw();

	for (;;) {
		if (pages == 0 || candidate + length > limit.raw()) return VirtAddr();

		bool moved = false;
		for (size_t idx = 0; idx < regionCount && !moved; idx++) {
			uint64_t region_start = regions[idx].virt_addr;
			uint64_t region_end =
				region_start +
				(static_cast<uint64_t>(regions[idx].page_count) << hal::PAGE_SHIFT);
			if (candidate < region_end && region_start < candidate + length) {
				candidate = region_end;
				moved = true;
			}
		}
		if (!moved) return VirtAddr(static_cast<hal::VirtualRaw>(candidate));
	}
}

} // namespace cock::core::task
