#ifndef COCK_USER_CRT0_HPP
#define COCK_USER_CRT0_HPP

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
 * Runs the static constructors (`cock::user::runConstructors`), calls `main` and ends the
 * process with `exit_process` and the value `main` returned. It is linked into every
 * program by `userland/common.mk` and never returns.
 */
extern "C" [[noreturn]] void _start();

#endif
