#include <cock/core/irq/irq_routing.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using irq::IrqRouting;
using security::CapabilityType;
using task::Scheduler;
using task::Thread;
using utils::Logger;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;
constexpr uint64_t SINGLE_LINE = 1;

uintptr_t sys_irq_bind(uintptr_t line, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;

	if (!current->getProcess()->getCapabilities().covers(CapabilityType::IRQ,
														 line, SINGLE_LINE)) {
		Logger::warn("sys_irq_bind: PID %d has no capability for line %u",
					 current->getProcess()->getId(),
					 static_cast<unsigned>(line));
		return FAILURE;
	}
	return IrqRouting::bind(static_cast<uint32_t>(line), current->getId())
			   ? SUCCESS
			   : FAILURE;
}

uintptr_t sys_irq_ack(uintptr_t line, uintptr_t, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	return IrqRouting::acknowledge(static_cast<uint32_t>(line), current->getId())
			   ? SUCCESS
			   : FAILURE;
}

} // namespace cock::core::syscall
