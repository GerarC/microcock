#include <cock/core/hal/tasking.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/page.hpp>

namespace cock::core::syscall {

using task::PendingProcesses;
using task::Process;
using task::Scheduler;
using task::Thread;

static_assert(abi::process::PENDING_STACK_BYTES == task::USER_STACK_SIZE,
			  "the user loader and the kernel must agree on the size of the user stack");
static_assert(abi::process::PENDING_PAGE_BYTES == hal::PAGE_SIZE,
			  "the user loader and the kernel must agree on the page size");

PendingSpaceScope::PendingSpaceScope(const Process &process)
	: previous(hal::get_current_address_space()) {
	hal::switch_address_space(process.getAddressSpace());
}

PendingSpaceScope::~PendingSpaceScope() { hal::switch_address_space(previous); }

PendingProcesses::Entry *pending_of_caller(uintptr_t handle) {
	Thread *current = Scheduler::getCurrentThread();
	if (current == nullptr) return nullptr;
	return PendingProcesses::find(static_cast<uint32_t>(handle), current->getProcess());
}

bool image_range_fits(const Process &process, uintptr_t address, uint64_t bytes) {
	uint64_t image_limit = process.getUserSpaceLimit().raw() - task::USER_STACK_SIZE;
	if (bytes == 0 || address < hal::PAGE_SIZE || address >= image_limit) return false;
	return bytes <= image_limit - address;
}

bool image_pages_fit(const Process &process, uintptr_t address, uintptr_t pages) {
	if ((address & hal::PAGE_MASK) != 0 || pages == 0) return false;
	if (pages > (abi::process::MAX_PENDING_IMAGE_BYTES >> hal::PAGE_SHIFT)) return false;
	return image_range_fits(process, address, static_cast<uint64_t>(pages) << hal::PAGE_SHIFT);
}

} // namespace cock::core::syscall
