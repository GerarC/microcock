/**
 * @file pipe.hpp
 * @brief Client of the pipe server, over synchronous IPC calls.
 *
 * Each request is one `ep_call` of four words through the handle on the endpoint of the server that the name
 * server gave the caller; the server tells its clients apart by the badge of the handle. A call blocks until the
 * server answers, and a `READ` or `WRITE` the server cannot serve yet (the pipe is empty or full) is answered
 * when the pipe changes. The ends the server gave are numbers local to the process.
 */
#ifndef COCK_USER_PIPE_HPP
#define COCK_USER_PIPE_HPP

#include <cock/abi/pipe.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user::pipe {

/** Pause between tries of `read` and `write` while the server has no room to hold the call. */
constexpr uint32_t BUSY_POLL_MS = 10;
/** Value returned by `read` and `write` on failure. */
constexpr long FAILED = -1;

/** Connection to the pipe server. `connect` is not thread safe; after it, threads may call `read`, `write` and `close` at the same time. */
class Client {
  public:
	/**
	 * @brief Connects to the pipe server through the name server.
	 *
	 * Reuses the known server while it is alive, otherwise connects again, so it can be called after the server
	 * restarted (the ends it gave are gone then).
	 * @return true when a server is known; false when no process has the pipe name or the manifest does not let
	 *         the caller talk to it.
	 */
	bool connect() {
		if (server != NO_PROCESS && process_alive(server)) return true;
		names::Connection connection = names::connect(abi::pipe::SERVER_NAME);
		server = connection.pid;
		handle = connection.handle;
		return server != NO_PROCESS;
	}

	/**
	 * @brief Makes a pipe. Blocks for the answer.
	 * @param readEnd Receives the number of the read end.
	 * @param writeEnd Receives the number of the write end.
	 * @return true when the pipe was made; false with `lastStatus()` set (`NO_ROOM`, `NOT_CONNECTED`).
	 */
	bool create(uint32_t &readEnd, uint32_t &writeEnd) {
		uint32_t words[CALL_WORDS] = {};
		words[abi::pipe::HEADER_WORD] = abi::pipe::pack(static_cast<uint32_t>(abi::pipe::Op::CREATE), 0);

		abi::pipe::Status answered;
		CallResult result = request(words, answered);
		if (!result.ok || answered != abi::pipe::Status::OK) return false;

		readEnd = result.words[abi::pipe::FIRST_DATA_WORD];
		writeEnd = result.words[abi::pipe::SECOND_WORD];
		return true;
	}

	/**
	 * @brief Takes bytes out of a pipe. Blocks while the pipe is empty and a write end is open.
	 * @param end Read end.
	 * @param buffer Destination.
	 * @param length Capacity of the buffer, capped at `CHUNK_BYTES`; `0` returns `0` at once.
	 * @return The bytes read, `0` at the end of the data (empty, and no write end open), or `FAILED` with
	 *         `lastStatus()` set (`BAD_HANDLE`, `NOT_CONNECTED`...).
	 */
	long read(uint32_t end, uint8_t *buffer, size_t length) {
		if (length == 0) return 0;
		if (length > abi::pipe::CHUNK_BYTES) length = abi::pipe::CHUNK_BYTES;

		for (;;) {
			uint32_t words[CALL_WORDS] = {};
			words[abi::pipe::HEADER_WORD] =
				abi::pipe::pack(static_cast<uint32_t>(abi::pipe::Op::READ), length, end);

			abi::pipe::Status answered;
			CallResult result = request(words, answered);
			if (!result.ok) return FAILED;
			if (answered == abi::pipe::Status::BUSY) {
				sleep_ms(BUSY_POLL_MS);
				continue;
			}
			if (answered != abi::pipe::Status::OK) return FAILED;

			size_t count = abi::pipe::lengthOf(result.words[abi::pipe::HEADER_WORD]);
			if (count > length) return fail(abi::pipe::Status::BAD_REQUEST);
			abi::pipe::unpackBytes(result.words, buffer, count);
			return static_cast<long>(count);
		}
	}

	/**
	 * @brief Puts bytes in a pipe, in chunks of `CHUNK_BYTES`. Blocks while the pipe is full.
	 * @param end Write end.
	 * @param bytes Source.
	 * @param length Number of bytes.
	 * @return The bytes written; `FAILED` with `lastStatus()` set when the first chunk failed. When a later chunk
	 *         fails the count written so far is returned (a short write), and the next call reports the failure
	 *         (`BROKEN` when no read end is open, `BAD_HANDLE`, `NOT_CONNECTED`...).
	 */
	long write(uint32_t end, const uint8_t *bytes, size_t length) {
		size_t done = 0;
		while (done < length) {
			size_t step = length - done;
			if (step > abi::pipe::CHUNK_BYTES) step = abi::pipe::CHUNK_BYTES;

			uint32_t words[CALL_WORDS] = {};
			words[abi::pipe::HEADER_WORD] =
				abi::pipe::pack(static_cast<uint32_t>(abi::pipe::Op::WRITE), step, end);
			abi::pipe::packBytes(words, bytes + done, step);

			abi::pipe::Status answered;
			CallResult result = request(words, answered);
			if (!result.ok) break;
			if (answered == abi::pipe::Status::BUSY) {
				sleep_ms(BUSY_POLL_MS);
				continue;
			}
			if (answered != abi::pipe::Status::OK) break;

			size_t count = abi::pipe::lengthOf(result.words[abi::pipe::HEADER_WORD]);
			if (count == 0 || count > step) {
				fail(abi::pipe::Status::BAD_REQUEST);
				break;
			}
			done += count;
		}
		return done == 0 && length != 0 ? FAILED : static_cast<long>(done);
	}

	/**
	 * @brief Closes an end. Blocks for the answer.
	 * @param end The end.
	 * @return true when the server closed it; false with `lastStatus()` set.
	 */
	bool close(uint32_t end) {
		uint32_t words[CALL_WORDS] = {};
		words[abi::pipe::HEADER_WORD] =
			abi::pipe::pack(static_cast<uint32_t>(abi::pipe::Op::CLOSE), 0, end);

		abi::pipe::Status answered;
		CallResult result = request(words, answered);
		return result.ok && answered == abi::pipe::Status::OK;
	}

	/**
	 * @brief Status of the last call.
	 * @return The status of the answer, or `NOT_CONNECTED` when the call did not go through.
	 */
	abi::pipe::Status lastStatus() const {
		return status;
	}

  private:
	/**
	 * @brief Sends one request through the handle on the pipe endpoint and keeps the status of the answer.
	 *
	 * The status of this call is returned in `answered`, so threads that call at the same time do not read each
	 * other's; `lastStatus()` keeps the one of whichever call ended last.
	 * @param words The four request words.
	 * @param answered Receives the status of the answer, or `NOT_CONNECTED` when the call did not go through.
	 * @return The call result.
	 */
	CallResult request(const uint32_t (&words)[CALL_WORDS], abi::pipe::Status &answered) {
		CallResult result = {};
		if (handle != abi::NO_CAPABILITY_NODE) result = ep_call(handle, words);

		answered = result.ok ? static_cast<abi::pipe::Status>(
								   abi::pipe::codeOf(result.words[abi::pipe::HEADER_WORD]))
							 : abi::pipe::Status::NOT_CONNECTED;
		status = answered;
		return result;
	}

	/**
	 * @brief Records a status and gives the failure value.
	 * @param value The status to keep.
	 * @return `FAILED`.
	 */
	long fail(abi::pipe::Status value) {
		status = value;
		return FAILED;
	}

	/** Process id of the pipe server, `NO_PROCESS` before `connect`. */
	uint32_t server = NO_PROCESS;
	/** Handle on the endpoint of the pipe server, `NO_CAPABILITY_NODE` when there is none. */
	uint32_t handle = abi::NO_CAPABILITY_NODE;
	/** Status of the last call. */
	abi::pipe::Status status = abi::pipe::Status::OK;
};

} // namespace cock::user::pipe

#endif // !COCK_USER_PIPE_HPP
