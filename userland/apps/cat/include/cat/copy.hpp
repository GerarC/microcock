/** @file
 * @brief Copying a file to the standard output.
 */
#ifndef CAT_COPY_HPP
#define CAT_COPY_HPP

namespace cock::cat {

/** @brief Descriptor of the standard input. */
constexpr int STDIN_DESCRIPTOR = 0;

/** @brief Descriptor of the standard output. */
constexpr int STDOUT_DESCRIPTOR = 1;

/**
 * @brief Writes the bytes of a file to the standard output, as they are.
 *
 * @param program Name of the program, for the messages.
 * @param path Path as typed; the C library resolves it against the working directory.
 * @return 0 when the whole file was copied; 1 after printing `<program>: <path>: <reason>` when it cannot be opened,
 * read or written.
 */
int copyFile(const char *program, const char *path);

/**
 * @brief Writes the standard input to the standard output until it ends.
 *
 * The input is whatever descriptor 0 is: the console, a file (`cat < path`) or the read end of a pipe.
 *
 * @param program Name of the program, for the messages.
 * @return 0 when the input ended; 1 after printing `<program>: standard input: <reason>` when it cannot be read or the
 * output cannot be written.
 */
int copyStandardInput(const char *program);

} // namespace cock::cat

#endif // !CAT_COPY_HPP
