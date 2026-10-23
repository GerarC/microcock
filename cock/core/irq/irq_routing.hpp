/** @file
 * @brief Routing of hardware interrupt lines to the Ring 3 threads that own
 * them.
 */
#ifndef CORE_IRQ_ROUTING_HPP
#define CORE_IRQ_ROUTING_HPP

#include <hal/interrupt.hpp>
#include <stdint.h>

namespace cock::core::irq {

/**
 * @brief Table that maps each interrupt line to at most one owner thread.
 *
 * When a bound line fires, the line is masked and the owner is notified
 * through IPC. The owner acknowledges to unmask it again. All state lives in
 * static storage, the class is never instantiated.
 */
class IrqRouting {
  public:
	/**
	 * @brief Binds a line to a thread and unmasks it.
	 * @param line Interrupt line. Reserved lines (timer and others) are
	 * refused.
	 * @param owner_thread_id Id of the thread that will receive the
	 * notifications.
	 * @return True on success. False when the line is reserved or already
	 * owned.
	 * @note Takes an InterruptGuard. Range checking of the line is left to
	 * the HAL reservation test.
	 */
	static bool bind(uint32_t line, uint32_t owner_thread_id);

	/**
	 * @brief Unmasks a line after its owner has serviced the device.
	 * @param line Interrupt line.
	 * @param owner_thread_id Thread making the request.
	 * @return True on success. False when the line is reserved or the caller
	 * is not its owner.
	 * @note Takes an InterruptGuard.
	 */
	static bool acknowledge(uint32_t line, uint32_t owner_thread_id);

	/**
	 * @brief Drops every binding of a thread and masks those lines.
	 * @param owner_thread_id Thread being destroyed.
	 * @note Takes an InterruptGuard. Safe to call for a thread with no lines.
	 */
	static void release(uint32_t owner_thread_id);

	/**
	 * @brief Tells which thread owns a line.
	 * @param line Interrupt line.
	 * @param owner_thread_id Receives the thread id of the owner.
	 * @return True when the line is in range and owned. False otherwise, and
	 * `owner_thread_id` is not written.
	 * @note Takes an InterruptGuard.
	 */
	static bool ownerOf(uint32_t line, uint32_t &owner_thread_id);

	/**
	 * @brief Drops the binding of one line and masks it, whoever owns it.
	 * @param line Interrupt line. A line out of range or unowned changes nothing.
	 * @note Takes an InterruptGuard. Used when the capability for the line is
	 * revoked.
	 */
	static void unbind(uint32_t line);

	/**
	 * @brief Delivers an interrupt to the owner of its line.
	 *
	 * Masks the line and sends the owner an asynchronous notification whose
	 * payload is the line number. If the notification cannot be queued the
	 * binding is dropped.
	 *
	 * @param line Interrupt line that fired.
	 * @return True when an owner was notified, so the scheduler should yield.
	 * False when the line is out of range, unowned or the notification failed.
	 * @note Called from the interrupt handler, with interrupts already
	 * disabled.
	 */
	static bool forward(uint32_t line);

  private:
	/** @brief Owner thread id plus one for each line, zero meaning free. */
	static uint32_t ownerSlots[hal::IRQ_LINE_COUNT];
};

} // namespace cock::core::irq

#endif // !CORE_IRQ_ROUTING_HPP
