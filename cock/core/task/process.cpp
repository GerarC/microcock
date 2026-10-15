#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/process.hpp>
#include <hal/address_space.hpp>

namespace cock::core::task {

using memory::PhysAddr;
using memory::VirtAddr;
using memory::VirtualMemoryManager;

Process::Process(PhysAddr address_space, VirtAddr user_space_limit,
				 bool is_kernel)
	: kernel(is_kernel), addressSpace(address_space),
	  userSpaceLimit(user_space_limit),
	  mmapCursor(VirtAddr(hal::USER_MMAP_BASE)) {
	hal::init_process_context(context);
}

Process::~Process() {
	if (!kernel) VirtualMemoryManager::destroyAddressSpace(addressSpace);
}

Process *Process::createKernel() {
	return new Process(VirtualMemoryManager::getKernelDirectory(), VirtAddr(),
					   true);
}

Process *Process::create(PhysAddr address_space, VirtAddr user_space_limit) {
	return new Process(address_space, user_space_limit, false);
}

void Process::attachThread(uint32_t thread_id) {
	if (threadCount == 0) id = thread_id;
	threadCount++;
}

bool Process::detachThread() {
	if (threadCount > 0) threadCount--;
	return threadCount == 0;
}

void Process::addMemoryRegion(VirtAddr addr, size_t pages) {
	mappedRegions.append({addr.raw(), pages});
}

bool Process::takeMemoryRegion(VirtAddr addr, size_t pages) {
	auto found = mappedRegions.find({addr.raw(), 0});
	if (found == mappedRegions.end() || (*found).page_count != pages)
		return false;
	mappedRegions.remove({addr.raw(), 0});
	return true;
}

} // namespace cock::core::task
