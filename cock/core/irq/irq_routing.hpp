#ifndef CORE_IRQ_ROUTING_HPP
#define CORE_IRQ_ROUTING_HPP

#include <hal/interrupt.hpp>
#include <stdint.h>

namespace cock::core::irq {

class IrqRouting {
  public:
	static bool bind(uint32_t line, uint32_t owner_thread_id);
	static bool acknowledge(uint32_t line, uint32_t owner_thread_id);
	static void release(uint32_t owner_thread_id);

	static bool forward(uint32_t line);

  private:
	static uint32_t ownerSlots[hal::IRQ_LINE_COUNT];
};

} // namespace cock::core::irq

#endif // !CORE_IRQ_ROUTING_HPP
