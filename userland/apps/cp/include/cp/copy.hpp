/** @file
 * @brief Copying the content of one file into another.
 */
#ifndef CP_COPY_HPP
#define CP_COPY_HPP

namespace cock::cp {

/**
 * @brief Copies `from` into `to`, which is created when it does not exist and emptied when it does.
 *
 * Both paths are resolved against the working directory by the C library. Copying a file onto itself fails, because
 * the file server lets only one writer open a file and the source is opened first.
 *
 * @param program Name of the program, for the messages.
 * @param from Path of the source.
 * @param to Path of the destination.
 * @return 0 when everything was copied; 1 after printing `<program>: <path>: <reason>` naming the file that failed.
 */
int copyFile(const char *program, const char *from, const char *to);

} // namespace cock::cp

#endif // !CP_COPY_HPP
