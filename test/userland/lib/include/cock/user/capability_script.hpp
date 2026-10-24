/**
 * @file capability_script.hpp
 * @brief What `capgiver` and `capreceiver` agree on: the ports they pass around, the tags of the
 * transfers and the stages of the conversation.
 *
 * The giver holds the ports `GIVER_FIRST_PORT` to `GIVER_FIRST_PORT + GIVER_PORT_COUNT - 1` and the IRQ line
 * `GIVER_IRQ_LINE`, both with the grant flag (manifest `caps=io:0x2F8+8*,irq:10*`). The receiver holds nothing
 * and accepts only `IO_PORTS` capabilities.
 */
#ifndef TEST_USER_CAPABILITY_SCRIPT_HPP
#define TEST_USER_CAPABILITY_SCRIPT_HPP

#include <stdint.h>

namespace cock::user::capability_script {

/** Name of the receiver in the manifest, used by the giver to connect. */
constexpr char RECEIVER_NAME[] = "capreceiver";

/** First port of the range the giver holds. */
constexpr uint32_t GIVER_FIRST_PORT = 0x2F8;

/** Number of ports of the range the giver holds. */
constexpr uint32_t GIVER_PORT_COUNT = 8;

/** Ports of the first transfer, which cannot be passed on. */
constexpr uint32_t FIRST_TRANSFER_PORT = GIVER_FIRST_PORT;

/** Number of ports of each transfer. */
constexpr uint32_t TRANSFER_PORT_COUNT = 2;

/** First port of the second transfer, which can be passed on. */
constexpr uint32_t SECOND_TRANSFER_PORT = FIRST_TRANSFER_PORT + TRANSFER_PORT_COUNT;

/** A port past the range the giver holds. */
constexpr uint32_t UNHELD_PORT = GIVER_FIRST_PORT + GIVER_PORT_COUNT;

/** A port inside the range of the giver that no transfer covers. */
constexpr uint32_t UNTRANSFERRED_PORT = SECOND_TRANSFER_PORT + TRANSFER_PORT_COUNT;

/** Count of a single port or line. */
constexpr uint32_t SINGLE_UNIT = 1;

/** Interrupt line the giver holds and the receiver does not accept. */
constexpr uint32_t GIVER_IRQ_LINE = 10;

/** Tag (`arg5`) of the first transfer. */
constexpr uint32_t FIRST_TAG = 0xA1;

/** Tag (`arg5`) of the second transfer. */
constexpr uint32_t SECOND_TAG = 0xB2;

/** Value of `arg0` of the ping that tells the receiver both transfers were sent. */
constexpr uint32_t STAGE_SENT = 1;

/** Value of `arg0` of the ping that tells the receiver the first transfer was revoked. */
constexpr uint32_t STAGE_REVOKED = 2;

/** Value of `arg0` of a pong whose sender found everything as expected. */
constexpr uint32_t ANSWER_OK = 1;

/** Value of `arg0` of a pong whose sender found something wrong. */
constexpr uint32_t ANSWER_WRONG = 0;

/** Time the giver waits for each answer, in milliseconds. */
constexpr uint32_t ANSWER_WAIT_MILLISECONDS = 200;

/** Times the giver and the receiver wait for each other before giving up. */
constexpr uint32_t WAIT_ROUNDS = 100;

/** Exit status of a program that found everything as expected. */
constexpr int EXIT_PASSED = 0;

/** Exit status of a program that found something wrong. */
constexpr int EXIT_FAILED = 1;

} // namespace cock::user::capability_script

#endif // !TEST_USER_CAPABILITY_SCRIPT_HPP
