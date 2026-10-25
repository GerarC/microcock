/** @file
 * @brief The PS/2 keyboard controller: its data port and its interrupt line.
 */
#ifndef KBD_CONTROLLER_HPP
#define KBD_CONTROLLER_HPP

#include <cock/ipc/message.hpp>
#include <stdint.h>

namespace cock::kbd {

/**
 * @brief Takes the data port and binds the keyboard interrupt line.
 *
 * Needs the port and interrupt capabilities that `rs` grants, so it must run after
 * `CAPABILITY_GRANTED`.
 *
 * @return True when both succeed, false when a capability is missing or the line is taken.
 */
bool openController();

/**
 * @brief Tells whether a message is the keyboard interrupt notice from the kernel.
 *
 * @param message Message received.
 * @return True for an `IRQ_NOTIFICATION` sent by the kernel, false for anything else.
 */
bool isInterrupt(const cock::ipc::Message &message);

/**
 * @brief Reads the scancode waiting in the controller.
 *
 * @return The raw byte. Only valid after `openController` succeeded.
 */
uint8_t readScancode();

/**
 * @brief Acknowledges the keyboard interrupt so the kernel delivers the next one.
 */
void acknowledge();

} // namespace cock::kbd

#endif // !KBD_CONTROLLER_HPP
