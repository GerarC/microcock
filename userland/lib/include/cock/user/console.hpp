/**
 * @file console.hpp
 * @brief Client of the console driver (output) and of the `tty` server (input), over synchronous IPC calls.
 *
 * Each request is one `ipc_call` of four words: a header (operation, length, flag) plus
 * packed bytes. The server is found through the name server, which gives the caller a handle
 * on the endpoint of the server; every call goes through that handle, and the server tells its
 * clients apart by the badge of the handle. Writing and clearing go to the console driver; reading goes to the `tty`
 * server, which the client finds the first time a program reads. All calls block until the server answers.
 */
#ifndef COCK_USER_CONSOLE_HPP
#define COCK_USER_CONSOLE_HPP

#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user::console {

/** Pause between polls of `read` while no input is available. */
constexpr uint32_t READ_POLL_MS = 10;
/** Value returned by `write` and `read` on failure. */
constexpr long FAILED = -1;

/** Connection to the console driver and, once a program reads, to the `tty` server. Not thread safe. */
class Client {
  public:
	/**
	 * @brief Connects to the console server through the name server.
	 *
	 * Reuses the known server while it is alive, otherwise connects again, so it can
	 * be called after the server restarted. Only the first thread of a process can connect;
	 * another thread uses the connection the first one made.
	 * @return true when a server is known; false when no process has the console name or the
	 *         manifest does not let the caller talk to it.
	 */
	bool connect() {
		if (server != NO_PROCESS && process_alive(server)) return true;
		names::Connection connection = names::connect(abi::console::SERVER_NAME);
		server = connection.pid;
		handle = connection.handle;
		return server != NO_PROCESS;
	}

	/**
	 * @brief Writes text to the screen, in chunks of `CHUNK_BYTES`. Blocks per chunk.
	 * @param text Bytes to write.
	 * @param length Number of bytes.
	 * @return The number of bytes written; `FAILED` when the first chunk failed. When a
	 *         later chunk fails the count written so far is returned (a short write).
	 *         Fails when not connected or the server died or refused the chunk.
	 */
	long write(const char *text, size_t length) {
		size_t written = 0;
		while (written < length) {
			size_t step = length - written;
			if (step > abi::console::CHUNK_BYTES) step = abi::console::CHUNK_BYTES;

			uint32_t words[CALL_WORDS] = {};
			bool last = written + step == length;
			words[abi::console::HEADER_WORD] =
				abi::console::pack(static_cast<uint32_t>(abi::console::Op::WRITE), step, last);
			abi::console::packBytes(words, text + written, step);

			CallResult result = request(words);
			if (!result.ok || !accepted(result, step)) return written == 0 ? FAILED : static_cast<long>(written);
			written += step;
		}
		return static_cast<long>(written);
	}

	/**
	 * @brief Clears the screen. Blocks for the answer.
	 * @return true when the server cleared it; false on any failure.
	 */
	bool clear() {
		uint32_t words[CALL_WORDS] = {};
		words[abi::console::HEADER_WORD] =
			abi::console::pack(static_cast<uint32_t>(abi::console::Op::CLEAR), 0);

		CallResult result = request(words);
		return result.ok && statusOf(result) == abi::console::Status::OK;
	}

	/**
	 * @brief Draws bytes of the typed line on the screen only. Blocks for the answer.
	 *
	 * Only the `tty` server may do it; the driver answers `DENIED` to anyone else.
	 * @param text Bytes to draw; a backspace erases the character before the cursor.
	 * @param length Number of bytes, at most `CHUNK_BYTES`.
	 * @return true when the driver drew them; false on any failure.
	 */
	bool echo(const char *text, size_t length) {
		if (length == 0 || length > abi::console::CHUNK_BYTES) return false;

		uint32_t words[CALL_WORDS] = {};
		words[abi::console::HEADER_WORD] =
			abi::console::pack(static_cast<uint32_t>(abi::console::Op::ECHO), length);
		abi::console::packBytes(words, text, length);

		CallResult result = request(handle, words);
		return result.ok && accepted(result, length);
	}

	/**
	 * @brief Reads input from the keyboard through the `tty` server. Blocks until bytes exist.
	 *
	 * Asks for up to `length` bytes (capped at `CHUNK_BYTES`). While the server has no
	 * input, or says it is busy with another reader, the call sleeps `READ_POLL_MS` and
	 * asks again, so it never returns zero.
	 * @param buffer Destination.
	 * @param length Capacity of the buffer; `0` returns `0` at once.
	 * @return The number of bytes read (at least 1), or `FAILED` when no `tty` server can be reached (the manifest
	 *         has to list `tty` in `ipc=`), the call failed, the server returned an error or it reported more bytes
	 *         than requested.
	 */
	long read(char *buffer, size_t length) {
		if (length == 0) return 0;
		if (!connectTty()) return FAILED;
		if (length > abi::console::CHUNK_BYTES) length = abi::console::CHUNK_BYTES;

		for (;;) {
			uint32_t words[CALL_WORDS] = {};
			words[abi::console::HEADER_WORD] =
				abi::console::pack(static_cast<uint32_t>(abi::console::Op::READ), length);

			CallResult result = request(ttyHandle, words);
			if (!result.ok) return FAILED;
			if (statusOf(result) == abi::console::Status::BUSY) {
				sleep_ms(READ_POLL_MS);
				continue;
			}
			if (statusOf(result) != abi::console::Status::OK) return FAILED;

			size_t count = abi::console::lengthOf(result.words[abi::console::HEADER_WORD]);
			if (count > length) return FAILED;
			if (count > 0) {
				abi::console::unpackBytes(result.words, buffer, count);
				return static_cast<long>(count);
			}
			sleep_ms(READ_POLL_MS);
		}
	}

  private:
	/**
	 * @brief Connects to the `tty` server the first time and again after it died.
	 * @return true when a `tty` server is known.
	 */
	bool connectTty() {
		if (ttyServer != NO_PROCESS && process_alive(ttyServer)) return true;
		names::Connection connection = names::connect(abi::console::TTY_NAME);
		ttyServer = connection.pid;
		ttyHandle = connection.handle;
		return ttyServer != NO_PROCESS;
	}

	/**
	 * @brief Sends one request through a handle on a server endpoint that the name server gave.
	 * @param target The handle: `handle` for the driver, `ttyHandle` for the `tty` server.
	 * @param words The four request words.
	 * @return The call result; `FAILED` when not connected.
	 */
	CallResult request(uint32_t target, const uint32_t (&words)[CALL_WORDS]) {
		if (target == abi::NO_CAPABILITY_NODE) {
			CallResult failed = {};
			failed.status = CallStatus::FAILED;
			return failed;
		}
		return ep_call(target, words);
	}

	CallResult request(const uint32_t (&words)[CALL_WORDS]) { return request(handle, words); }

	/**
	 * @brief Extracts the status code from the header word of an answer.
	 * @param result Successful call result.
	 * @return The server status.
	 */
	static abi::console::Status statusOf(const CallResult &result) {
		return static_cast<abi::console::Status>(
			abi::console::codeOf(result.words[abi::console::HEADER_WORD]));
	}

	/**
	 * @brief Checks that the server accepted exactly the bytes sent.
	 * @param result Successful call result.
	 * @param expected Number of bytes in the request.
	 * @return true when the status is OK and the length echoed equals `expected`.
	 */
	static bool accepted(const CallResult &result, size_t expected) {
		return statusOf(result) == abi::console::Status::OK &&
			   abi::console::lengthOf(result.words[abi::console::HEADER_WORD]) == expected;
	}

	/** Process id of the console driver, `NO_PROCESS` before `connect`. */
	uint32_t server = NO_PROCESS;
	/** Handle on the endpoint of the console driver, `NO_CAPABILITY_NODE` when there is none. */
	uint32_t handle = abi::NO_CAPABILITY_NODE;
	/** Process id of the `tty` server, `NO_PROCESS` before the first read. */
	uint32_t ttyServer = NO_PROCESS;
	/** Handle on the endpoint of the `tty` server, `NO_CAPABILITY_NODE` when there is none. */
	uint32_t ttyHandle = abi::NO_CAPABILITY_NODE;
};

} // namespace cock::user::console

#endif // !COCK_USER_CONSOLE_HPP
