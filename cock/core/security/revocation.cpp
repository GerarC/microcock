#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/irq/irq_routing.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/revocation.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::security {

using hal::InterruptGuard;
using irq::IrqRouting;
using memory::NO_DEVICE_BASE;
using memory::PhysAddr;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using task::Process;
using task::TaskManager;
using task::Thread;

namespace {

constexpr uint64_t SINGLE_RESOURCE = 1;

Process *processHolding(uint32_t holder) {
	Thread *thread = TaskManager::getThreadByPID(holder);
	if (thread == nullptr || thread->getProcess()->getId() != holder) return nullptr;
	return thread->getProcess();
}

template <typename Action>
void forEachUncovered(const CapabilitySet &capabilities, CapabilityType type,
					  uint64_t first, uint64_t count, Action action) {
	uint64_t run_start = 0;
	bool open = false;
	for (uint64_t value = first; value < first + count; value++) {
		bool covered = capabilities.covers(type, value, SINGLE_RESOURCE);
		if (!covered && !open) {
			run_start = value;
			open = true;
		} else if (covered && open) {
			action(run_start, value - run_start);
			open = false;
		}
	}
	if (open) action(run_start, first + count - run_start);
}

void closePorts(Process &holder, const CapabilityNode &node) {
	forEachUncovered(holder.getCapabilities(), CapabilityType::IO_PORTS, node.first,
					 node.count, [&holder](uint64_t first, uint64_t count) {
						 hal::revoke_hardware_resource(
							 holder.getContext(), static_cast<uint32_t>(first),
							 static_cast<uint32_t>(count));
					 });

	if (hal::get_current_address_space().raw() == holder.getAddressSpace().raw())
		hal::apply_process_context(holder.getContext());
}

void unbindLines(Process &holder, const CapabilityNode &node) {
	forEachUncovered(
		holder.getCapabilities(), CapabilityType::IRQ, node.first, node.count,
		[&holder](uint64_t first, uint64_t count) {
			for (uint64_t line = first; line < first + count; line++) {
				uint32_t owner = 0;
				if (!IrqRouting::ownerOf(static_cast<uint32_t>(line), owner)) continue;
				Thread *thread = TaskManager::getThreadByPID(owner);
				if (thread != nullptr && thread->getProcess() == &holder)
					IrqRouting::unbind(static_cast<uint32_t>(line));
			}
		});
}

void unmapDevices(Process &holder, const CapabilityNode &node) {
	uintptr_t starts[Process::MAX_MAPPING_REGIONS];
	size_t lengths[Process::MAX_MAPPING_REGIONS];
	size_t found = 0;

	for (size_t idx = 0; idx < holder.getRegionCount(); idx++) {
		memory::MemoryRegion region = holder.getRegion(idx);
		if (region.device_base == NO_DEVICE_BASE) continue;

		uint64_t span = static_cast<uint64_t>(region.page_count) << hal::PAGE_SHIFT;
		bool touches = region.device_base < node.end() && node.first < region.device_base + span;
		if (!touches || holder.getCapabilities().covers(CapabilityType::PHYSICAL_MEMORY,
														 region.device_base, span))
			continue;
		starts[found] = region.virt_addr;
		lengths[found] = region.page_count;
		found++;
	}
	if (found == 0) return;

	PhysAddr previous = hal::get_current_address_space();
	bool switched = previous.raw() != holder.getAddressSpace().raw();
	if (switched) hal::switch_address_space(holder.getAddressSpace());
	for (size_t idx = 0; idx < found; idx++) {
		VirtualMemoryManager::freePages(VirtAddr(starts[idx]).as<void>(), lengths[idx]);
		holder.takeMemoryRegion(VirtAddr(starts[idx]), lengths[idx]);
	}
	if (switched) hal::switch_address_space(previous);
}

void undoEffects(Process &holder, const CapabilityNode &node) {
	switch (node.type) {
	case CapabilityType::IO_PORTS:
		closePorts(holder, node);
		break;
	case CapabilityType::IRQ:
		unbindLines(holder, node);
		break;
	case CapabilityType::PHYSICAL_MEMORY:
		unmapDevices(holder, node);
		break;
	default:
		break;
	}
}

} // namespace

bool revoke_capability(Process &caller, CapabilityNodeId node) {
	InterruptGuard guard;
	if (!capability_tree.hasAncestorHeldBy(node, caller.getId())) return false;

	CapabilityNodeId ids[CapabilityTree::MAX_NODES];
	size_t count = capability_tree.collectSubtree(node, ids, CapabilityTree::MAX_NODES);

	for (size_t idx = 0; idx < count; idx++) {
		const CapabilityNode *entry = capability_tree.find(ids[idx]);
		if (entry == nullptr) continue;
		Process *holder = processHolding(entry->holder);
		if (holder != nullptr) holder->getCapabilities().removeNode(ids[idx]);
	}

	for (size_t idx = 0; idx < count; idx++) {
		const CapabilityNode *entry = capability_tree.find(ids[idx]);
		if (entry == nullptr) continue;
		CapabilityNode removed = *entry;
		capability_tree.detach(ids[idx]);
		Process *holder = processHolding(removed.holder);
		if (holder != nullptr) undoEffects(*holder, removed);
	}
	return true;
}

} // namespace cock::core::security
