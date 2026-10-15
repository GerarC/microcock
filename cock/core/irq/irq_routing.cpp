#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/irq/irq_routing.hpp>
#include <cock/core/task/ipc.hpp>

namespace cock::core::irq {

using hal::InterruptGuard;

constexpr uint32_t NO_OWNER_SLOT = 0;

uint32_t IrqRouting::ownerSlots[hal::IRQ_LINE_COUNT];

bool IrqRouting::bind(uint32_t line, uint32_t owner_thread_id) {
	if (hal::irq_line_reserved(line)) return false;

	InterruptGuard guard;
	if (ownerSlots[line] != NO_OWNER_SLOT) return false;

	ownerSlots[line] = owner_thread_id + 1;
	hal::irq_unmask(line);
	return true;
}

bool IrqRouting::acknowledge(uint32_t line, uint32_t owner_thread_id) {
	if (hal::irq_line_reserved(line)) return false;

	InterruptGuard guard;
	if (ownerSlots[line] != owner_thread_id + 1) return false;

	hal::irq_unmask(line);
	return true;
}

void IrqRouting::release(uint32_t owner_thread_id) {
	InterruptGuard guard;
	for (uint32_t line = 0; line < hal::IRQ_LINE_COUNT; line++) {
		if (ownerSlots[line] != owner_thread_id + 1) continue;
		hal::irq_mask(line);
		ownerSlots[line] = NO_OWNER_SLOT;
	}
}

bool IrqRouting::forward(uint32_t line) {
	if (line >= hal::IRQ_LINE_COUNT || ownerSlots[line] == NO_OWNER_SLOT)
		return false;

	hal::irq_mask(line);

	if (!task::IPC::notify(ownerSlots[line] - 1, line)) {
		ownerSlots[line] = NO_OWNER_SLOT;
		return false;
	}
	return true;
}

} // namespace cock::core::irq
