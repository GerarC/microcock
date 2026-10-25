/**
 * @file file_fuzz.hpp
 * @brief Fuzzing of the file server protocol: well formed requests with hostile contents.
 *
 * The fuzzer opens a real session with a server, so the requests pass the first checks and reach the code that
 * reads paths, handles, offsets and lengths. It sends every `FS_*` request with lengths past the window, paths
 * with no terminator, dots, repeated separators and random bytes, handles that were never opened and flags that
 * are not defined. A server passes when it keeps answering a valid request afterwards.
 */
#ifndef FUZZ_FILE_FUZZ_HPP
#define FUZZ_FILE_FUZZ_HPP

#include <stdint.h>

namespace fuzz {

/** What happened to a server during a run. */
enum class FileFuzzOutcome : uint32_t {
	/** The name server refused the connection or the server has no session for the fuzzer: nothing was sent. */
	NOT_REACHABLE,
	/** The server answered a valid request after the run, and accepted a new session. */
	SURVIVED,
	/** The server died, or stopped answering a valid request. */
	LOST,
};

/** Counters of one run. */
struct FileFuzzReport {
	/** What happened to the server. */
	FileFuzzOutcome outcome;
	/** Requests sent. */
	uint32_t requests;
	/** Requests that got a reply. */
	uint32_t answered;
	/** Requests that got no reply in time while the server was alive. */
	uint32_t timedOut;
};

/**
 * @brief Sends hostile requests to one file server over its own session.
 * @param serverName Name of the server in the name server (`vfs`, `ramfs`, `fat`, `ext`).
 * @param seed Seed of the generator.
 * @param rounds Number of requests to send.
 * @return The counters and the outcome.
 */
FileFuzzReport runFileFuzz(const char *serverName, uint32_t seed, uint32_t rounds);

} // namespace fuzz

#endif // !FUZZ_FILE_FUZZ_HPP
