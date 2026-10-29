/** @file
 * @brief Reading whole files from the initrd through the file server.
 */
#ifndef RS_FILES_HPP
#define RS_FILES_HPP

#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::rs {

/** @brief Size and byte count of nothing. */
constexpr size_t NO_BYTES = 0;

/**
 * @brief Reads an open file from its start until `size` bytes are copied.
 *
 * @param client A connected file client.
 * @param handle The open file.
 * @param destination Receives `size` bytes.
 * @param size Number of bytes to read.
 * @return True when all `size` bytes arrived, false when the file ended or a read failed.
 */
bool readInto(cock::user::fs::Client &client, uint32_t handle, uint8_t *destination,
			  uint32_t size);

} // namespace cock::rs

#endif // !RS_FILES_HPP
