#ifndef COCK_USER_CRT0_HPP
#define COCK_USER_CRT0_HPP

#include <stdint.h>

/**
 * @brief The program's own entry, defined by every program as `extern "C" int main()`.
 *
 * It takes no arguments: the kernel gives a program nothing on its stack. Its return
 * value is the exit status that `_start` passes to `exit_process`. A server that never
 * leaves its loop never returns.
 */
extern "C" int main();

/**
 * @brief The entry point of every program (`ENTRY(_start)` in `userland/linker.ld`).
 *
 * Saves the value the kernel left in `EAX` (the initial handle, see below), runs the static
 * constructors (`cock::user::runConstructors`), calls `main` and ends the process with
 * `exit_process` and the value `main` returned. It is linked into every program by
 * `userland/common.mk` and never returns.
 */
extern "C" [[noreturn]] void _start();

/**
 * @brief What `_start` calls with the value it found in `EAX`: stores it for `cock::user::initialHandle`,
 * runs the static constructors, calls `main` and ends the process. Never returns.
 * @param handle The initial handle, zero when the process was started with none. A process that `exec`
 * started gets `abi::exec::ARGS_ADDRESS` instead, which no handle can equal: the argument page there holds
 * the initial handle of the replaced program and the arguments, and `startProgram` takes both from it.
 */
extern "C" [[noreturn]] void startProgram(uint32_t handle);

namespace cock::user {

/**
 * @brief The handle the process was born with: the number of an `ENDPOINT` capability that the
 * kernel derived from the spawner when it started the first thread (`SpawnRights::initialEndpoint`).
 *
 * `rs` makes it a handle on the name server whose badge is the process id of the program, so a
 * program can call `names::connect` from its first instruction, with no message to wait for.
 * @return The handle, or `abi::NO_CAPABILITY_NODE` (0) for a process that was started with none,
 * such as the boot modules.
 */
uint32_t initialHandle();

/**
 * @brief Number of arguments the program was started with.
 * @return The count, including the name the program was started with as the first one; zero for a program
 * that a spawn started, which gets none.
 */
uint32_t argumentCount();

/**
 * @brief One argument of a program that `exec` started.
 * @param index Position, from 0; the first is the name the program was started with.
 * @return The string, which lives as long as the process, or null when the index is past the last argument or
 * the program was not started by `exec`.
 */
const char *argument(uint32_t index);

} // namespace cock::user

#endif
