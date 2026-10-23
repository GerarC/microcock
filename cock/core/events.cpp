#include <cock/abi/process.hpp>
#include <cock/core/events.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/irq/irq_routing.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <hal/cpu.hpp>

namespace cock::core::events {

using core::syscall::SyscallManager;
using core::syscall::SystemCall;
using core::task::Scheduler;
using utils::Logger;

constexpr uintptr_t UNKNOWN_SYSCALL_RESULT = static_cast<uintptr_t>(-1);

SyscallOutcome on_syscall(const SyscallRequest &request,
						  uintptr_t stack_pointer) {
	Scheduler::setSyscallFrame(stack_pointer);
	SystemCall handler = SyscallManager::getSyscall(request.number);
	if (handler == nullptr)
		return {UNKNOWN_SYSCALL_RESULT, stack_pointer, true};

	uintptr_t value = handler(request.args[0], request.args[1], request.args[2],
							  request.args[3], request.args[4],
							  request.args[5]);

	bool deliver_value = true;
	if (Scheduler::takeRestartRequest()) {
		hal::rewind_syscall(stack_pointer);
		deliver_value = false;
	}
	if (Scheduler::takeRescheduleRequest())
		stack_pointer = Scheduler::onYield(stack_pointer);

	return {value, stack_pointer, deliver_value};
}

uintptr_t on_exception(const ExceptionInfo &info, uintptr_t stack_pointer) {
	Logger::error("Exception{ .idx: %u, .type: %s, .ring: %u}", info.vector,
				  info.name, info.from_user ? 3u : 0u);
	Logger::error("  -> EIP: 0x%x  Error Code: 0x%x",
				  static_cast<unsigned>(info.instruction), info.error_code);
	if (info.has_fault_address)
		Logger::error("  -> Faulting Address (CR2): 0x%x",
					  static_cast<unsigned>(info.fault_address));

	if (!info.from_user || !Scheduler::getCurrentThread())
		utils::panic("Unrecoverable exception in kernel mode");

	Logger::error("Thread %d killed by exception",
				  Scheduler::getCurrentThread()->getId());
	Scheduler::getCurrentThread()->getProcess()->setExitStatus(
		abi::process::FAULT_STATUS);
	Scheduler::terminateCurrentThread();
	return Scheduler::onYield(stack_pointer);
}

uintptr_t on_irq(uint32_t line, uintptr_t stack_pointer) {
	if (line == hal::TIMER_IRQ_LINE) return Scheduler::onTick(stack_pointer);
	if (irq::IrqRouting::forward(line)) return Scheduler::onYield(stack_pointer);
	return stack_pointer;
}

} // namespace cock::core::events
