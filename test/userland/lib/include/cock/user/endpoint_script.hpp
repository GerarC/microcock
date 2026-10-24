/**
 * @file endpoint_script.hpp
 * @brief What `epgiver` and `epserver` agree on: the badge, the tags and the stages of the conversation.
 *
 * The giver holds grant authority (manifest `caps=io:0x2E8+8*`), creates an endpoint, binds it to the
 * server and gives itself a handle with a chosen badge. The server holds nothing and accepts nothing; it
 * learns who spoke only from the badge the kernel writes.
 */
#ifndef TEST_USER_ENDPOINT_SCRIPT_HPP
#define TEST_USER_ENDPOINT_SCRIPT_HPP

#include <stdint.h>

namespace cock::user::endpoint_script {

/** Name of the server in the manifest, used by the giver to connect. */
constexpr char SERVER_NAME[] = "epserver";

/** Badge the giver puts on the handle it gives itself. */
constexpr uint32_t HANDLE_BADGE = 0x51;

/** Badge the giver writes in a message to try to forge one; the kernel replaces it. */
constexpr uint32_t FORGED_BADGE = 0x99;

/** Stage (word 0) of a call through the handle: the server must see the badge as the peer and echo the words. */
constexpr uint32_t STAGE_CALL = 1;

/** Stage of the call that tells the server to try what it must not be able to do; word 1 is the endpoint id. */
constexpr uint32_t STAGE_CHECK = 2;

/** Stage of the call that tells the server to judge the messages it received and finish. */
constexpr uint32_t STAGE_FINISH = 3;

/** Words 1 to 3 of the call through the handle. */
constexpr uint32_t FIRST_PAYLOAD = 0x1111;
constexpr uint32_t SECOND_PAYLOAD = 0x2222;
constexpr uint32_t THIRD_PAYLOAD = 0x3333;

/** Value of `arg0` of the message sent through the handle. */
constexpr uint32_t TAG_THROUGH_HANDLE = 0xA1;

/** Value of `arg0` of the message sent by process id. */
constexpr uint32_t TAG_BY_PROCESS_ID = 0xB2;

/** Value of `arg0` of the message sent through the owner handle after the endpoint was bound to the giver. */
constexpr uint32_t TAG_AFTER_REBIND = 0xC3;

/** Number of handle values the server tries one by one; every one must fail. */
constexpr uint32_t GUESS_COUNT = 256;

/** First handle value the server tries. */
constexpr uint32_t FIRST_GUESS = 1;

/** A handle value no process holds. */
constexpr uint32_t UNHELD_HANDLE = 0x7FFFFF00;

/** Word 0 of an answer whose sender found everything as expected. */
constexpr uint32_t ANSWER_OK = 1;

/** Word 0 of an answer whose sender found something wrong. */
constexpr uint32_t ANSWER_WRONG = 0;

/** Time the giver waits for a call that must fail, in milliseconds. */
constexpr uint32_t FAILING_CALL_MILLISECONDS = 50;

/** Times the giver tries to find the server before giving up. */
constexpr uint32_t WAIT_ROUNDS = 100;

/** Pause between those tries, in milliseconds. */
constexpr uint32_t ATTEMPT_MILLISECONDS = 50;

/** Exit status of a program that found everything as expected. */
constexpr int EXIT_PASSED = 0;

/** Exit status of a program that found something wrong. */
constexpr int EXIT_FAILED = 1;

} // namespace cock::user::endpoint_script

#endif // !TEST_USER_ENDPOINT_SCRIPT_HPP
