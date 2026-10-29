/** @file
 * @brief What the small programs of `/bin` share: exit statuses, argument counts and the failure message.
 *
 * Every program prints a failure as `<program>: <subject>: <reason>` on the console, with the reason taken from `errno`,
 * and a wrong count of arguments as `usage: <program> <arguments>`. The implementation is `userland/lib/src/tool.cpp`;
 * a program lists it in its `SRCS`.
 */
#ifndef COCK_USER_TOOL_HPP
#define COCK_USER_TOOL_HPP

#include <stddef.h>

namespace cock::user::tool {

/** @brief Exit status of a program that did its job. */
constexpr int STATUS_OK = 0;

/** @brief Exit status of a program that tried and failed. */
constexpr int STATUS_FAILURE = 1;

/** @brief Exit status of a program called with the wrong arguments. */
constexpr int STATUS_USAGE = 2;

/** @brief Index in `argv` of the first argument; index 0 is the name the program was started with. */
constexpr int FIRST_ARGUMENT = 1;

/** @brief Argument count of a call with no arguments. */
constexpr int NO_ARGUMENTS = 0;

/** @brief Argument count of a call with one argument. */
constexpr int ONE_ARGUMENT = 1;

/** @brief Argument count of a call with two arguments. */
constexpr int TWO_ARGUMENTS = 2;

/**
 * @brief Counts the arguments of a call.
 *
 * @param argc The count `main` received, the program name included.
 * @return The number of arguments after the name; 0 when `argc` is 0.
 */
inline int argumentCount(int argc) {
	return argc > NO_ARGUMENTS ? argc - FIRST_ARGUMENT : NO_ARGUMENTS;
}

/**
 * @brief Prints how a program is used.
 *
 * @param program Name of the program, as printed in front.
 * @param arguments Text that describes the arguments, such as `path` or `from to`.
 * @return `STATUS_USAGE`, so a caller can `return usage(...)`.
 */
int usage(const char *program, const char *arguments);

/**
 * @brief Turns an error number of the C library into words.
 *
 * @param code A value of `errno`.
 * @return Text such as `not found`, `not empty` or `read only`; `failed` for a code with no text of its own.
 */
const char *reasonFor(int code);

/**
 * @brief Prints that an operation on a subject failed, with the reason from `errno`.
 *
 * @param program Name of the program.
 * @param subject What failed: usually a path.
 * @return `STATUS_FAILURE`, so a caller can `return fail(...)`.
 */
int fail(const char *program, const char *subject);

} // namespace cock::user::tool

#endif // !COCK_USER_TOOL_HPP
