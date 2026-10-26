/** @file
 * @brief `hexdump [path]`: shows the bytes of a file in hexadecimal.
 */
#ifndef HEXDUMP_COMMAND_HPP
#define HEXDUMP_COMMAND_HPP

namespace cock::hexdump {

/**
 * @brief Prints a file, or the standard input when there is no path, sixteen bytes to a row: the offset in eight
 * hexadecimal digits, the bytes, and the same bytes between bars with `.` for those that are not printable. The last
 * line has the total size as an offset, as `hexdump -C` does.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and at most one path.
 * @return 0 when everything was printed; 1 after printing `hexdump: <path>: <reason>` when the input cannot be opened
 * or read or the output cannot be written; 2 after printing `usage: hexdump [path]` for more than one path.
 */
int run(int argc, char **argv);

} // namespace cock::hexdump

#endif // !HEXDUMP_COMMAND_HPP
