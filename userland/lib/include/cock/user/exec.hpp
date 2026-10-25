/**
 * @file exec.hpp
 * @brief `exec` for a program: replaces its own image with a program read from a file.
 */
#ifndef COCK_USER_EXEC_HPP
#define COCK_USER_EXEC_HPP

#include <cock/abi/pm.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/pm.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user {

/** Why `exec` came back. It comes back only when it failed. */
enum class ExecFailure : uint32_t {
	/** The file does not exist or cannot be opened. */
	NO_FILE,
	/** The file is empty, larger than `abi::pm::MAX_EXEC_IMAGE_BYTES`, or could not be read in full. */
	BAD_FILE,
	/** `pm` refused: not a loadable image, too many or too long arguments, or no room. `pm::Client::lastStatus` says which. */
	REFUSED,
};

/**
 * @brief Replaces the image of the calling process with the program in a file.
 *
 * Reads the file through `files` into an object that `process` made, and asks `pm` to put it in place of the
 * running image. On success this function does not return: the process starts again in the new program, with
 * the same process id, parent, capabilities and endpoints, with no other thread, and with `argv` as
 * `cock::user::argument`. A failure leaves the running image untouched.
 *
 * @param process Client connected to `pm`.
 * @param files Client connected to a file server that reaches the path.
 * @param path NUL terminated path of the program.
 * @param argv The arguments, `argc` strings; by convention the first is the name of the program.
 * @param argc Number of arguments.
 * @return Why it failed.
 */
inline ExecFailure exec(pm::Client &process, fs::Client &files, const char *path, const char *const *argv,
						uint32_t argc) {
	constexpr size_t NO_BYTES = 0;

	uint32_t handle = files.open(path);
	if (handle == abi::fs::NO_HANDLE) return ExecFailure::NO_FILE;

	fs::FileInfo info = {};
	ExecFailure failure = ExecFailure::BAD_FILE;
	if (files.stat(handle, info) && info.size > NO_BYTES && info.size <= abi::pm::MAX_EXEC_IMAGE_BYTES) {
		uint8_t *image = process.begin(info.size);
		if (image != nullptr && files.readAll(handle, image, info.size)) {
			const char *name = argc != 0 ? argv[0] : path;
			files.close(handle);
			process.finishExec(name, argv, argc, info.size);
			return ExecFailure::REFUSED;
		}
		process.release();
		if (image == nullptr) failure = ExecFailure::REFUSED;
	}
	files.close(handle);
	return failure;
}

} // namespace cock::user

#endif // !COCK_USER_EXEC_HPP
